package com.muonline.client;

import android.app.Activity;
import android.util.Log;

import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingClientStateListener;
import com.android.billingclient.api.BillingFlowParams;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.PendingPurchasesParams;
import com.android.billingclient.api.ProductDetails;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.PurchasesUpdatedListener;
import com.android.billingclient.api.QueryProductDetailsParams;
import com.android.billingclient.api.QueryPurchasesParams;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;

/**
 * Google Play Billing for the cash shop's WCoin recharge.
 *
 * Native code drives it through MuMainNativeActivity.billingFromNative, and
 * reads results by polling ("poll") rather than being called back, the same
 * shape as the banner download: Play's listeners run on the main thread and
 * the game reads them on its own.
 *
 * Purchases are NOT acknowledged or consumed here. That is the server's job
 * once it has verified the token with Google and credited the WCoin; Play
 * refunds a purchase that is never acknowledged within three days, so nothing
 * is lost if the server never sees it. Until then an owned product cannot be
 * bought again, and every connect re-reports it so the game can retry.
 */
final class MuBilling {
    private static final String TAG = "MuBilling";

    // Product ids, created in Play Console as one-time (consumable) products.
    static final String[] PRODUCT_IDS = {
        "wcoin_c_20", "wcoin_c_110", "wcoin_c_320",
        "wcoin_c_540", "wcoin_c_1100", "wcoin_c_2750"
    };

    private static BillingClient client;
    private static boolean connecting;
    private static final Map<String, ProductDetails> details = new ConcurrentHashMap<>();
    private static final ConcurrentLinkedQueue<String> results = new ConcurrentLinkedQueue<>();
    // Tokens already reported this run, so a reconnect does not report one twice.
    private static final Map<String, Boolean> reported = new ConcurrentHashMap<>();

    private MuBilling() {}

    static String handle(final Activity activity, String cmd, String arg) {
        if (activity == null || cmd == null) return "";
        switch (cmd) {
            case "init":
                activity.runOnUiThread(() -> connect(activity));
                return "";
            case "price": {
                ProductDetails d = details.get(arg);
                if (d == null || d.getOneTimePurchaseOfferDetails() == null) return "";
                return d.getOneTimePurchaseOfferDetails().getFormattedPrice();
            }
            case "buy":
                activity.runOnUiThread(() -> buy(activity, arg));
                return "";
            case "poll": {
                String r = results.poll();
                return r != null ? r : "";
            }
            default:
                return "";
        }
    }

    private static void push(String status, String productId, String token, String orderId) {
        results.add(status + "|" + nz(productId) + "|" + nz(token) + "|" + nz(orderId));
    }

    private static String nz(String s) {
        return s != null ? s.replace('|', '/') : "";
    }

    private static void connect(Activity activity) {
        if (client != null && client.isReady()) {
            loadProducts();
            reportOwned();
            return;
        }
        if (connecting) return;
        connecting = true;

        PurchasesUpdatedListener listener = (result, purchases) -> {
            int code = result.getResponseCode();
            if (code == BillingClient.BillingResponseCode.OK && purchases != null) {
                for (Purchase p : purchases) report(p);
            } else if (code == BillingClient.BillingResponseCode.USER_CANCELED) {
                push("cancelled", "", "", "");
            } else if (code == BillingClient.BillingResponseCode.ITEM_ALREADY_OWNED) {
                // Bought before but not credited yet: hand it over again.
                reportOwned();
            } else {
                push("error:" + result.getDebugMessage() + " (" + code + ")", "", "", "");
            }
        };

        client = BillingClient.newBuilder(activity.getApplicationContext())
            .setListener(listener)
            .enablePendingPurchases(PendingPurchasesParams.newBuilder().enableOneTimeProducts().build())
            .build();

        client.startConnection(new BillingClientStateListener() {
            @Override
            public void onBillingSetupFinished(BillingResult result) {
                connecting = false;
                if (result.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                    loadProducts();
                    reportOwned();
                } else {
                    Log.w(TAG, "setup failed: " + result.getDebugMessage());
                }
            }

            @Override
            public void onBillingServiceDisconnected() {
                connecting = false;
            }
        });
    }

    private static void loadProducts() {
        List<QueryProductDetailsParams.Product> list = new ArrayList<>();
        for (String id : PRODUCT_IDS) {
            list.add(QueryProductDetailsParams.Product.newBuilder()
                .setProductId(id)
                .setProductType(BillingClient.ProductType.INAPP)
                .build());
        }
        client.queryProductDetailsAsync(
            QueryProductDetailsParams.newBuilder().setProductList(list).build(),
            (result, queryResult) -> {
                // Billing 8 wraps the list; ids Play does not know come back
                // in getUnfetchedProductList instead.
                List<ProductDetails> productDetails =
                    queryResult != null ? queryResult.getProductDetailsList() : null;
                if (result.getResponseCode() != BillingClient.BillingResponseCode.OK || productDetails == null) {
                    Log.w(TAG, "products failed: " + result.getDebugMessage());
                    return;
                }
                for (ProductDetails d : productDetails) details.put(d.getProductId(), d);
                Log.i(TAG, "products loaded: " + details.size());
            });
    }

    private static void reportOwned() {
        client.queryPurchasesAsync(
            QueryPurchasesParams.newBuilder().setProductType(BillingClient.ProductType.INAPP).build(),
            (result, purchases) -> {
                if (result.getResponseCode() != BillingClient.BillingResponseCode.OK) return;
                for (Purchase p : purchases) {
                    if (!p.isAcknowledged()) report(p);
                }
            });
    }

    private static void report(Purchase p) {
        String productId = p.getProducts().isEmpty() ? "" : p.getProducts().get(0);
        if (p.getPurchaseState() == Purchase.PurchaseState.PENDING) {
            push("pending", productId, "", "");
            return;
        }
        if (p.getPurchaseState() != Purchase.PurchaseState.PURCHASED) return;
        if (reported.put(p.getPurchaseToken(), Boolean.TRUE) != null) return;
        push("ok", productId, p.getPurchaseToken(), p.getOrderId());
    }

    private static void buy(Activity activity, String productId) {
        if (client == null || !client.isReady()) {
            push("error:Google Play is not connected yet, try again", productId, "", "");
            connect(activity);
            return;
        }
        ProductDetails d = details.get(productId);
        if (d == null) {
            push("error:This pack is not available on Google Play", productId, "", "");
            loadProducts();
            return;
        }
        List<BillingFlowParams.ProductDetailsParams> params = new ArrayList<>();
        params.add(BillingFlowParams.ProductDetailsParams.newBuilder().setProductDetails(d).build());
        BillingResult r = client.launchBillingFlow(activity,
            BillingFlowParams.newBuilder().setProductDetailsParamsList(params).build());
        if (r.getResponseCode() != BillingClient.BillingResponseCode.OK) {
            push("error:" + r.getDebugMessage() + " (" + r.getResponseCode() + ")", productId, "", "");
        }
    }
}
