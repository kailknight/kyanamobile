// IosDeviceId.mm - this iPhone's id for device bans (GameServer DeviceBan.h).
//
// iOS has no ANDROID_ID equivalent that survives a reinstall:
// identifierForVendor resets once every app from this developer is deleted.
// So a random UUID is created once and kept in the Keychain, which iOS keeps
// across app deletion and reinstall (it is cleared by a full device erase).
//
// Sent exactly like Android's: SHA-256("kira-device:" + uuid), first 32 hex
// chars, appended to packet 0xD3:0x7D by SendClientBuild().
#import <Foundation/Foundation.h>
#import <Security/Security.h>
#include <CommonCrypto/CommonDigest.h>
#include <cstdio>
#include <cstring>

static NSString* const kService = @"com.worldofkira.device";
static NSString* const kAccount = @"device-id";

static NSString* LoadUuid()
{
    NSDictionary* query = @{
        (__bridge id)kSecClass:       (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: kService,
        (__bridge id)kSecAttrAccount: kAccount,
        (__bridge id)kSecReturnData:  @YES,
        (__bridge id)kSecMatchLimit:  (__bridge id)kSecMatchLimitOne,
    };

    CFTypeRef result = nullptr;
    if (SecItemCopyMatching((__bridge CFDictionaryRef)query, &result) != errSecSuccess || result == nullptr)
    {
        return nil;
    }

    NSData* data = (__bridge_transfer NSData*)result;
    return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
}

static bool StoreUuid(NSString* uuid)
{
    NSDictionary* item = @{
        (__bridge id)kSecClass:          (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService:    kService,
        (__bridge id)kSecAttrAccount:    kAccount,
        (__bridge id)kSecValueData:      [uuid dataUsingEncoding:NSUTF8StringEncoding],
        // Readable after the first unlock, and never synced to iCloud or
        // restored onto another phone - it identifies THIS device.
        (__bridge id)kSecAttrAccessible: (__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
    };

    return SecItemAdd((__bridge CFDictionaryRef)item, nullptr) == errSecSuccess;
}

extern "C" void MU_IosFillDeviceId(char* out33)
{
    out33[0] = 0;

    @autoreleasepool
    {
        NSString* uuid = LoadUuid();
        if (uuid.length == 0)
        {
            uuid = [[NSUUID UUID] UUIDString];
            if (!StoreUuid(uuid))
            {
                // No Keychain: send nothing rather than an id that changes on
                // every launch and could never be banned meaningfully.
                return;
            }
        }

        NSString* salted = [@"kira-device:" stringByAppendingString:uuid];
        NSData* bytes = [salted dataUsingEncoding:NSUTF8StringEncoding];

        unsigned char hash[CC_SHA256_DIGEST_LENGTH];
        CC_SHA256(bytes.bytes, (CC_LONG)bytes.length, hash);

        for (int i = 0; i < 16; i++)
        {
            snprintf(out33 + i * 2, 3, "%02x", hash[i]);
        }
        out33[32] = 0;
    }
}
