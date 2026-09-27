package com.muonline.client;

import android.app.NativeActivity;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.AssetManager;
import android.graphics.Color;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Bundle;
import android.text.InputType;
import android.util.Log;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.concurrent.atomic.AtomicInteger;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputConnectionWrapper;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class MuMainNativeActivity extends NativeActivity {
    private static final String TAG = "CHATIME";
    private static MuMainNativeActivity instance;

    static {
        System.loadLibrary("main");
    }

    private static native void nativeOnTextInput(String text);
    private static native void nativeOnKeyEvent(
        int action,
        int keyCode,
        int unicodeChar,
        int metaState,
        int repeatCount);
    private native void nativeSetKeyboardBridge();
    private native void nativeClearKeyboardBridge();
    private static native void nativeOnWindowFocusChanged(boolean hasFocus);

    private BridgeEditText imeBridge;

    // onBackPressed() never fires here: NativeActivity delivers key events to
    // the native AInputQueue directly, a separate pipeline from the normal
    // View/Activity key dispatch that onBackPressed hangs off of. sokol_app's
    // Android backend reads BACK straight from that queue and used to call
    // its own shutdown the instant it saw AKEYCODE_BACK (see the note in
    // SokolRuntime.cpp, where that shutdown call is neutralised) - but even
    // with that neutralised, an unhandled event from the queue is just
    // dropped, it does not fall back to Java.
    //
    // dispatchKeyEvent is the one point every key event passes through in
    // Java before the framework hands it anywhere else, so intercepting BACK
    // here and returning true consumes it outright - it never reaches the
    // native queue at all, and never reaches the default NativeActivity
    // behaviour of finishing the activity. Forwarded through the same native
    // key bridge every other key already uses: the game decides what back
    // means (close the open window, or ask for confirmation when nothing is
    // open), never the framework.
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            try {
                nativeOnKeyEvent(
                    event.getAction(),
                    KeyEvent.KEYCODE_BACK,
                    0,
                    event.getMetaState(),
                    event.getRepeatCount());
            } catch (UnsatisfiedLinkError e) {
                // Native library not up yet - still consume below rather
                // than let it fall through to the default finish().
            }
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    public void showKeyboardFromBridge() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                showKeyboardInternal();
            }
        });
    }

    public void hideKeyboardFromBridge() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                hideKeyboardInternal();
            }
        });
    }

    public static void showKeyboardFromNative() {
        final MuMainNativeActivity activity = instance;
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                activity.showKeyboardInternal();
            }
        });
    }

    public static void hideKeyboardFromNative() {
        final MuMainNativeActivity activity = instance;
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                activity.hideKeyboardInternal();
            }
        });
    }

    /*
     * Cash shop banner download.
     *
     * The PC client uses urlmon on a worker thread; Android has neither that
     * nor the bundled curl (which is a Windows .lib), so the fetch lives here,
     * where HttpsURLConnection already exists and threading is easy.
     *
     * Asynchronous for the same reason as PC: native calls this from the packet
     * handler, and a blocking fetch there froze the client on the first shop
     * open. Native starts it and then polls.
     *
     * 0 = idle or still running, 1 = the file is on disk, -1 = failed. The
     * state is consumed by the poll so each outcome is reported exactly once.
     */
    private static final AtomicInteger bannerState = new AtomicInteger(0);
    private static volatile boolean bannerRunning = false;

    public static boolean startBannerDownloadFromNative(String url, String destPath) {
        if (url == null || destPath == null || url.length() == 0 || destPath.length() == 0) {
            return false;
        }

        // One at a time. A second shop open while the first fetch is in flight
        // would otherwise race two writers onto the same file.
        synchronized (bannerState) {
            if (bannerRunning) {
                return false;
            }
            bannerRunning = true;
            bannerState.set(0);
        }

        final String fUrl = url;
        final String fPath = destPath;

        Thread t = new Thread(new Runnable() {
            @Override
            public void run() {
                boolean ok = false;
                File tmp = new File(fPath + ".part");
                HttpURLConnection conn = null;

                try {
                    File parent = tmp.getParentFile();
                    if (parent != null) {
                        parent.mkdirs();
                    }

                    conn = (HttpURLConnection) new URL(fUrl).openConnection();
                    conn.setConnectTimeout(15000);
                    conn.setReadTimeout(20000);
                    conn.setInstanceFollowRedirects(true);
                    conn.setRequestProperty("User-Agent", "MuClient");
                    conn.connect();

                    if (conn.getResponseCode() == HttpURLConnection.HTTP_OK) {
                        InputStream in = conn.getInputStream();
                        FileOutputStream out = new FileOutputStream(tmp);
                        try {
                            byte[] buf = new byte[8192];
                            int n;
                            while ((n = in.read(buf)) > 0) {
                                out.write(buf, 0, n);
                            }
                            out.flush();
                            ok = true;
                        } finally {
                            try { out.close(); } catch (Exception ignored) { }
                            try { in.close(); } catch (Exception ignored) { }
                        }
                    }
                } catch (Exception e) {
                    Log.w("MuBanner", "banner download failed: " + e);
                    ok = false;
                } finally {
                    if (conn != null) {
                        conn.disconnect();
                    }
                }

                /*
                 * Download to <path>.part and rename only on success.
                 *
                 * The native side treats "the file exists" as "it is cached and
                 * good", so a half-written file left by a dropped connection
                 * would be loaded forever as a corrupt banner and never
                 * re-fetched.
                 */
                if (ok) {
                    File dest = new File(fPath);
                    dest.delete();
                    ok = tmp.renameTo(dest);
                }

                if (!ok) {
                    tmp.delete();
                }

                bannerState.set(ok ? 1 : -1);
                bannerRunning = false;
            }
        }, "MuBannerDownload");

        t.setDaemon(true);
        t.start();
        return true;
    }

    public static int pollBannerDownloadFromNative() {
        return bannerState.getAndSet(0);
    }

    // Sticky-broadcast read rather than a registered receiver: ACTION_BATTERY_CHANGED
    // is always latched by the system, so passing a null receiver to
    // registerReceiver returns the last broadcast immediately without needing to
    // keep a receiver registered for the activity's whole lifetime. Needs no
    // permission. Returns -1 if the status bar HUD polls before the first
    // broadcast lands (practically never).
    // ---- proximity voice chat: RECORD_AUDIO --------------------------------
    //
    // The manifest declaring RECORD_AUDIO is not enough from Android 6 onward;
    // the player has to grant it while the app is running. Until they do,
    // OpenSL's CreateAudioRecorder simply fails with no detail, which from the
    // native side is indistinguishable from a device that has no microphone.
    //
    // Asked for only when somebody first tries to talk, rather than at startup.
    // A game that demands microphone access before the player has seen a reason
    // for it mostly gets told no, and a denial is much harder to walk back than
    // a prompt that arrives with obvious context.

    private static final int REQUEST_CODE_RECORD_AUDIO = 0x5643;   // 'VC'

    public static int hasMicPermissionFromNative() {
        final MuMainNativeActivity activity = instance;
        if (activity == null) {
            return 0;
        }
        // Below Android 6 a manifest permission is granted at install time and
        // there is nothing to ask for.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return 1;
        }
        try {
            return (activity.checkSelfPermission(android.Manifest.permission.RECORD_AUDIO)
                    == android.content.pm.PackageManager.PERMISSION_GRANTED) ? 1 : 0;
        } catch (Exception e) {
            Log.w(TAG, "mic permission check failed", e);
            return 0;
        }
    }

    public static void requestMicPermissionFromNative() {
        final MuMainNativeActivity activity = instance;
        if (activity == null) {
            return;
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return;
        }
        // Fire and forget. The result arrives asynchronously in the system
        // dialog, so native does not wait on it - it polls
        // hasMicPermissionFromNative on the next attempt to talk. Blocking the
        // game thread on a dialog the player may leave sitting there would
        // freeze the client.
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    activity.requestPermissions(
                            new String[] { android.Manifest.permission.RECORD_AUDIO },
                            REQUEST_CODE_RECORD_AUDIO);
                } catch (Exception e) {
                    Log.w(TAG, "mic permission request failed", e);
                }
            }
        });
    }

    public static int getBatteryPercentFromNative() {
        final MuMainNativeActivity activity = instance;
        if (activity == null) {
            return -1;
        }
        try {
            IntentFilter filter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
            Intent battery = activity.registerReceiver(null, filter);
            if (battery == null) {
                return -1;
            }
            int level = battery.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
            int scale = battery.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
            if (level < 0 || scale <= 0) {
                return -1;
            }
            return Math.round(level * 100.0f / scale);
        } catch (Exception e) {
            return -1;
        }
    }

    // RSSI in dBm (typically -30 "excellent" to -90 "unusable"). Needs
    // ACCESS_WIFI_STATE (install-time only, no runtime prompt). Returns
    // Integer.MIN_VALUE when wifi is off/disconnected so the native side can
    // tell "no signal" apart from a real weak-signal reading.
    public static int getWifiRssiFromNative() {
        final MuMainNativeActivity activity = instance;
        if (activity == null) {
            return Integer.MIN_VALUE;
        }
        try {
            WifiManager wifiManager =
                (WifiManager) activity.getApplicationContext().getSystemService(WIFI_SERVICE);
            if (wifiManager == null) {
                return Integer.MIN_VALUE;
            }
            WifiInfo info = wifiManager.getConnectionInfo();
            if (info == null || info.getNetworkId() == -1) {
                return Integer.MIN_VALUE;
            }
            return info.getRssi();
        } catch (Exception e) {
            return Integer.MIN_VALUE;
        }
    }

    private void showKeyboardInternal() {
        if (imeBridge == null) {
            return;
        }
        imeBridge.setFocusable(true);
        imeBridge.setFocusableInTouchMode(true);
        imeBridge.setVisibility(View.VISIBLE);
        imeBridge.requestFocusFromTouch();
        imeBridge.requestFocus();
        imeBridge.bringToFront();
        getWindow().setSoftInputMode(
            WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE
                | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING);

        final InputMethodManager imm =
            (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
        if (imm == null) {
            return;
        }

        imeBridge.post(new Runnable() {
            @Override
            public void run() {
                imeBridge.requestFocusFromTouch();
                imeBridge.requestFocus();
                imm.restartInput(imeBridge);
                imm.viewClicked(imeBridge);

                boolean shown =
                    imm.showSoftInput(imeBridge, InputMethodManager.SHOW_FORCED);
                if (!shown) {
                    imm.toggleSoftInput(InputMethodManager.SHOW_FORCED, 0);
                }

                Log.i(
                    TAG,
                    "showKeyboardInternal shown=" + shown
                        + " active=" + imm.isActive(imeBridge)
                        + " accepting=" + imm.isAcceptingText());
            }
        });
    }

    private void hideKeyboardInternal() {
        if (imeBridge == null) {
            return;
        }
        final InputMethodManager imm =
            (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
        if (imm != null) {
            imm.hideSoftInputFromWindow(imeBridge.getWindowToken(), 0);
            View decorView = getWindow().getDecorView();
            if (decorView != null) {
                imm.hideSoftInputFromWindow(decorView.getWindowToken(), 0);
            }
        }
        imeBridge.clearFocus();
        imeBridge.setVisibility(View.GONE);
        View decorView = getWindow().getDecorView();
        if (decorView != null) {
            decorView.setFocusableInTouchMode(true);
            decorView.requestFocus();
        }
        getWindow().setSoftInputMode(
            WindowManager.LayoutParams.SOFT_INPUT_STATE_HIDDEN
                | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING);
        Log.i(TAG, "hideKeyboardInternal");
    }

    private void resetImeBridgeBuffer() {
        if (imeBridge == null) {
            return;
        }
        imeBridge.setText("");
        imeBridge.setSelection(imeBridge.getText().length());
    }

    private void installImeBridge() {
        if (imeBridge != null) {
            return;
        }

        imeBridge = new BridgeEditText(this);
        imeBridge.setBackgroundColor(Color.TRANSPARENT);
        imeBridge.setTextColor(Color.TRANSPARENT);
        imeBridge.setHighlightColor(Color.TRANSPARENT);
        imeBridge.setCursorVisible(false);
        imeBridge.setLongClickable(false);
        imeBridge.setTextIsSelectable(false);
        imeBridge.setFocusable(true);
        imeBridge.setFocusableInTouchMode(true);
        imeBridge.setSingleLine(true);
        imeBridge.setVisibility(View.GONE);
        imeBridge.setInputType(
            InputType.TYPE_CLASS_TEXT
                | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
                | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD);
        imeBridge.setImeOptions(
            EditorInfo.IME_FLAG_NO_EXTRACT_UI
                | EditorInfo.IME_FLAG_NO_FULLSCREEN
                | EditorInfo.IME_ACTION_DONE);

        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(1, 1);
        params.leftMargin = 0;
        params.topMargin = 0;
        addContentView(imeBridge, params);
    }

    private final class BridgeEditText extends EditText {
        BridgeEditText(MuMainNativeActivity activity) {
            super(activity);
        }

        private void forwardKeyEvent(KeyEvent event) {
            if (event == null) {
                return;
            }
            nativeOnKeyEvent(
                event.getAction(),
                event.getKeyCode(),
                event.getUnicodeChar(),
                event.getMetaState(),
                event.getRepeatCount());
        }

        @Override
        public boolean onCheckIsTextEditor() {
            return true;
        }

        @Override
        public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
            final InputConnection baseConnection = super.onCreateInputConnection(outAttrs);

            // After super, not before. TextView.onCreateInputConnection assigns
            // outAttrs.imeOptions from the widget's own state, so flags set
            // beforehand were being overwritten and thrown away. In landscape an
            // IME defaults to fullscreen extract mode unless NO_FULLSCREEN
            // survives, which is why the keyboard took over the whole screen.
            outAttrs.imeOptions |= EditorInfo.IME_FLAG_NO_EXTRACT_UI
                | EditorInfo.IME_FLAG_NO_FULLSCREEN;
            return new InputConnectionWrapper(baseConnection, true) {
                @Override
                public boolean commitText(CharSequence text, int newCursorPosition) {
                    if (text != null && text.length() > 0) {
                        nativeOnTextInput(text.toString());
                    }
                    post(new Runnable() {
                        @Override
                        public void run() {
                            resetImeBridgeBuffer();
                        }
                    });
                    return true;
                }

                @Override
                public boolean setComposingText(CharSequence text, int newCursorPosition) {
                    if (text != null && text.length() > 0) {
                        nativeOnTextInput(text.toString());
                    }
                    post(new Runnable() {
                        @Override
                        public void run() {
                            resetImeBridgeBuffer();
                        }
                    });
                    return true;
                }

                @Override
                public boolean deleteSurroundingText(int beforeLength, int afterLength) {
                    if (beforeLength > 0) {
                        nativeOnKeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL, 0, 0, 0);
                        nativeOnKeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL, 0, 0, 0);
                        return true;
                    }
                    return super.deleteSurroundingText(beforeLength, afterLength);
                }

                @Override
                public boolean sendKeyEvent(KeyEvent event) {
                    forwardKeyEvent(event);
                    return true;
                }

                @Override
                public boolean performEditorAction(int actionCode) {
                    // The keyboard's Done key arrives here, not as a key event,
                    // because the bridge is single-line with IME_ACTION_DONE.
                    // Sending it on as text gave the game a newline character
                    // and never reached its SDLK_RETURN handler, so chat could
                    // be typed but not submitted or closed. Forward a real
                    // Enter key press instead - nativeOnKeyEvent already maps
                    // AKEYCODE_ENTER to SDLK_RETURN.
                    nativeOnKeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER, 0, 0, 0);
                    nativeOnKeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER, 0, 0, 0);
                    return true;
                }
            };
        }

        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event) {
            forwardKeyEvent(event);
            return true;
        }

        @Override
        public boolean onKeyUp(int keyCode, KeyEvent event) {
            forwardKeyEvent(event);
            return true;
        }

        @Override
        public boolean onKeyPreIme(int keyCode, KeyEvent event) {
            if (keyCode == KeyEvent.KEYCODE_BACK) {
                forwardKeyEvent(event);
                return true;
            }
            return super.onKeyPreIme(keyCode, event);
        }
    }


    // Re-reads the whole asset into memory to compare against what's already
    // on disk - fine here since every file under ui/ and data/ is small (the
    // two folders together are under 1MB), and it means this stays correct
    // for a workflow where these get swapped for new art/scripts repeatedly
    // without needing a versionCode bump to notice each change (a version-
    // gated "only re-copy once per app version" check was tried first and
    // discarded for exactly that reason - it left content-only rebuilds
    // silently stuck on stale files, which was already the whole bug).
    private void copyAssetFile(AssetManager assetMgr, String srcAssetPath, File destFile) {
        byte[] assetBytes;
        try (InputStream in = assetMgr.open(srcAssetPath)) {
            assetBytes = readAllBytes(in);
        } catch (IOException ex) {
            return;
        }

        if (destFile.exists() && destFile.length() == assetBytes.length
                && filesContentEqual(assetBytes, destFile)) {
            return;
        }

        File parent = destFile.getParentFile();
        if (parent != null && !parent.exists()) {
            parent.mkdirs();
        }
        try (OutputStream out = new FileOutputStream(destFile)) {
            out.write(assetBytes);
        } catch (IOException ignored) {
        }
    }

    private static byte[] readAllBytes(InputStream in) throws IOException {
        java.io.ByteArrayOutputStream buffer = new java.io.ByteArrayOutputStream();
        byte[] chunk = new byte[8192];
        int len;
        while ((len = in.read(chunk)) > 0) {
            buffer.write(chunk, 0, len);
        }
        return buffer.toByteArray();
    }

    private static boolean filesContentEqual(byte[] assetBytes, File destFile) {
        try (InputStream in = new FileInputStream(destFile)) {
            return java.util.Arrays.equals(readAllBytes(in), assetBytes);
        } catch (IOException ex) {
            return false;
        }
    }

    private void copyAssetFolder(AssetManager assetMgr, String srcFolder, File destDir) {
        String[] entries;
        try {
            entries = assetMgr.list(srcFolder);
        } catch (IOException e) {
            return;
        }
        if (entries == null || entries.length == 0) {
            return;
        }
        if (!destDir.exists()) {
            destDir.mkdirs();
        }
        for (String name : entries) {
            String childSrc = srcFolder + "/" + name;
            File childDst = new File(destDir, name);
            String[] sub;
            try {
                sub = assetMgr.list(childSrc);
            } catch (IOException ex) {
                sub = null;
            }
            if (sub != null && sub.length > 0) {
                copyAssetFolder(assetMgr, childSrc, childDst);
            } else {
                copyAssetFile(assetMgr, childSrc, childDst);
            }
        }
    }

    private void extractGameAssets() {
        File extDir = getExternalFilesDir(null);
        if (extDir == null) {
            extDir = getFilesDir();
        }
        copyAssetFolder(getAssets(), "ui", new File(extDir, "ui"));
        // Cash shop category/package/product script: CListManager::LoadScriptList
        // (GameShop/ShopListManager/ListManager.cpp) skips its WinINet-only live
        // downloader entirely when these files already exist locally, so shipping
        // them here is what makes the shop show real content on Android.
        copyAssetFolder(getAssets(), "data", new File(extDir, "data"));
    }

    private void configureFullscreenWindow() {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
            | WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().setSoftInputMode(
            WindowManager.LayoutParams.SOFT_INPUT_STATE_HIDDEN
                | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            WindowManager.LayoutParams attrs = getWindow().getAttributes();
            attrs.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(attrs);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
        }

        getWindow().setStatusBarColor(Color.TRANSPARENT);
        getWindow().setNavigationBarColor(Color.TRANSPARENT);
        applyImmersiveFlags();
    }

    private void applyImmersiveFlags() {
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        extractGameAssets();
        configureFullscreenWindow();
        super.onCreate(savedInstanceState);
        instance = this;
        nativeSetKeyboardBridge();
        installImeBridge();
    }

    @Override
    protected void onDestroy() {
        nativeClearKeyboardBridge();
        if (instance == this) {
            instance = null;
        }
        super.onDestroy();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        // super first, deliberately: NativeActivity forwards this to sokol,
        // which posts its own MSG_NO_FOCUS. Ours has to land behind that one.
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            configureFullscreenWindow();
        }
        // Losing focus is not the same as going to the background, and sokol's
        // frame loop treats it as if it were - it parks until the next system
        // message, so a floating window the player taps outside of freezes
        // mid-game. The native side keeps it running and mutes it instead; a
        // real background trip still stops both, through onPause.
        nativeOnWindowFocusChanged(hasFocus);
    }
}
