// App Store WCoin (C) recharge for the iPhone/iPad build: the StoreKit 2 side
// of MU_MobileBilling (5.Main/source/Platform/MobilePlatform.cpp), the iOS
// counterpart of android/.../MuBilling.java. Same text commands, same poll
// strings, so the cash shop drives both stores the same way.
//
//   "account" - arg = the logged-in account id. Every purchase carries a UUID
//               derived from it (appAccountToken), and the DataServer only
//               credits a transaction whose UUID matches the account reporting
//               it: Apple transaction ids are short numbers, so without this
//               anyone could claim someone else's purchase by guessing its id.
//   "init"    - load the WCoin products and re-report unfinished purchases
//   "price"   - arg = product id, out = the App Store's localised price, or ""
//   "buy"     - arg = product id, shows Apple's payment sheet
//   "poll"    - out = "status|productId|token|orderId" or ""; the token for a
//               paid purchase is "apple-<transaction id>"
//   "finish"  - arg = product id: the server credited (or had already
//               credited) the purchase reported for it, so finish it with Apple
//   "release" - arg = product id: the server could not credit it now; leave it
//               unfinished and report it again on the next shop open
//
// Purchases are finished only after the server has credited them, the same
// rule as Google's consume: an unfinished consumable is delivered again on
// every launch (Transaction.unfinished), so nothing is lost if the server never
// answers. One purchase per product is with the server at a time, because the
// server's answer names the product but not the transaction.

import Foundation
import StoreKit
import CryptoKit

private final class MuStore {
    static let shared = MuStore()

    // Product ids, created in App Store Connect as consumable in-app purchases.
    // The same ids as Google Play, so PlayBilling.ini serves both stores.
    let productIds = ["wcoin_c_20", "wcoin_c_110", "wcoin_c_320",
                      "wcoin_c_540", "wcoin_c_1100", "wcoin_c_2750"]

    private let lock = NSLock()
    private var products: [String: Product] = [:]
    private var results: [String] = []
    private var accountId = ""
    // Verified, unfinished transactions by id.
    private var unfinished: [UInt64: Transaction] = [:]
    // Per product: the transaction with the server now, and those waiting.
    private var inFlight: [String: UInt64] = [:]
    private var waiting: [String: [UInt64]] = [:]
    private var updates: Task<Void, Never>?
    private var loadingProducts = false

    private func locked<T>(_ body: () -> T) -> T {
        lock.lock()
        defer { lock.unlock() }
        return body()
    }

    private func push(_ status: String, _ productId: String = "", _ token: String = "", _ orderId: String = "") {
        let clean = { (s: String) in s.replacingOccurrences(of: "|", with: "/") }
        let line = "\(clean(status))|\(clean(productId))|\(clean(token))|\(clean(orderId))"
        locked { results.append(line) }
    }

    func handle(_ cmd: String, _ arg: String) -> String {
        switch cmd {
        case "account":
            locked { accountId = arg }
            return ""
        case "init":
            start()
            return ""
        case "price":
            return locked { products[arg]?.displayPrice ?? "" }
        case "buy":
            Task { await buy(arg) }
            return ""
        case "poll":
            return locked { results.isEmpty ? "" : results.removeFirst() }
        case "finish":
            finish(arg)
            return ""
        case "release":
            locked {
                inFlight[arg] = nil
                waiting[arg] = nil
            }
            return ""
        default:
            return ""
        }
    }

    // MARK: - Products and unfinished purchases

    private func start() {
        if updates == nil {
            // Purchases that complete outside the sheet: Ask to Buy approvals,
            // interrupted payments, a purchase finished on another device.
            updates = Task.detached { [weak self] in
                for await result in Transaction.updates {
                    self?.accept(result)
                }
            }
        }
        // Every shop open hands unfinished purchases to the server again; the
        // server dedupes by token, so a repeat costs nothing.
        locked {
            inFlight.removeAll()
            waiting.removeAll()
        }
        Task {
            await loadProducts()
            for await result in Transaction.unfinished {
                accept(result)
            }
        }
    }

    private func loadProducts() async {
        let alreadyLoading: Bool = locked {
            if loadingProducts || products.count == productIds.count { return true }
            loadingProducts = true
            return false
        }
        if alreadyLoading { return }
        defer { locked { loadingProducts = false } }
        do {
            let loaded = try await Product.products(for: productIds)
            locked {
                for p in loaded { products[p.id] = p }
            }
            NSLog("MuStore: %d products loaded", loaded.count)
        } catch {
            NSLog("MuStore: products failed: %@", error.localizedDescription)
        }
    }

    // MARK: - Purchases

    private func buy(_ productId: String) async {
        await loadProducts()
        guard let product = locked({ products[productId] }) else {
            push("error:This pack is not available on the App Store", productId)
            return
        }
        guard let token = accountToken() else {
            push("error:Log in to your game account first", productId)
            return
        }
        do {
            let result = try await product.purchase(options: [.appAccountToken(token)])
            switch result {
            case .success(let verification):
                accept(verification)
            case .userCancelled:
                push("cancelled", productId)
            case .pending:
                // Ask to Buy or a payment that needs action: Transaction.updates
                // delivers it once it goes through.
                push("pending", productId)
            @unknown default:
                push("error:The App Store returned an unknown result", productId)
            }
        } catch {
            push("error:\(error.localizedDescription)", productId)
        }
    }

    private func accept(_ verification: VerificationResult<Transaction>) {
        guard case .verified(let transaction) = verification else {
            // StoreKit could not verify it on the device; the server asks Apple
            // anyway, but an unverified transaction is not worth reporting.
            NSLog("MuStore: unverified transaction ignored")
            return
        }
        guard transaction.productType == .consumable, productIds.contains(transaction.productID) else {
            return
        }
        if transaction.revocationDate != nil {
            // Refunded before it was credited: nothing to pay out, clear it.
            Task { await transaction.finish() }
            return
        }

        let productId = transaction.productID
        let report: Bool = locked {
            unfinished[transaction.id] = transaction
            if inFlight[productId] == transaction.id || waiting[productId, default: []].contains(transaction.id) {
                return false
            }
            if inFlight[productId] == nil {
                inFlight[productId] = transaction.id
                return true
            }
            waiting[productId, default: []].append(transaction.id)
            return false
        }
        if report {
            push("ok", productId, "apple-\(transaction.id)", String(transaction.id))
        }
    }

    private func finish(_ productId: String) {
        let (done, next): (Transaction?, Transaction?) = locked {
            guard let id = inFlight[productId] else { return (nil, nil) }
            let done = unfinished.removeValue(forKey: id)
            inFlight[productId] = nil
            var next: Transaction?
            while next == nil, var queue = waiting[productId], !queue.isEmpty {
                let nextId = queue.removeFirst()
                waiting[productId] = queue
                next = unfinished[nextId]
            }
            if let next = next {
                inFlight[productId] = next.id
            }
            return (done, next)
        }
        if let done = done {
            Task { await done.finish() }
        }
        if let next = next {
            push("ok", productId, "apple-\(next.id)", String(next.id))
        }
    }

    // MARK: - Account binding

    // UUID from SHA-256("worldofkira:" + lowercased account id): the first 16
    // bytes, with the version-4/variant bits set. PlayBilling.cpp in the
    // DataServer derives the same value and compares it with the transaction's
    // appAccountToken.
    private func accountToken() -> UUID? {
        let account = locked { accountId }.lowercased()
        if account.isEmpty { return nil }
        var bytes = Array(SHA256.hash(data: Data(("worldofkira:" + account).utf8)).prefix(16))
        bytes[6] = (bytes[6] & 0x0F) | 0x40
        bytes[8] = (bytes[8] & 0x3F) | 0x80
        return UUID(uuid: (bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
                           bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]))
    }
}

// C entry point for MobilePlatform.cpp. Returns false only when there is no
// store (never on iOS 15+).
@_cdecl("MU_IosBilling")
public func MU_IosBilling(_ cmd: UnsafePointer<CChar>?, _ arg: UnsafePointer<CChar>?,
                          _ out: UnsafeMutablePointer<CChar>?, _ outSize: Int32) -> Bool {
    guard let cmd = cmd else { return false }
    let result = MuStore.shared.handle(String(cString: cmd), arg.map { String(cString: $0) } ?? "")
    if let out = out, outSize > 0 {
        let bytes = Array(result.utf8.prefix(Int(outSize) - 1))
        for (i, b) in bytes.enumerated() { out[i] = CChar(bitPattern: b) }
        out[bytes.count] = 0
    }
    return true
}
