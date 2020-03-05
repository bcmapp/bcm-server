#include "../test_common.h"
#include <utils/sender_utils.h>
#include <signal/curve.h>
#include <signal/signal_protocol.h>
#include <openssl/evp.h>
#include "../../tools/signal_openssl_provider.h"
#include <boost/beast/core/detail/base64.hpp>
#include <openssl/sha.h>
#include <crypto/evp_cipher.h>
#include <crypto/base64.h>
#include "../../src/proto/dao/client_version.pb.h"
#include "../../src/proto/dao/device.pb.h"
#include <config/encrypt_sender.h>
#include <thread>

using namespace bcm;

void testEncryptSender() 
{
    signal_context* context = nullptr;
    ec_key_pair* keyPair = nullptr;

    REQUIRE(signal_context_create(&context, 0) == 0);
    REQUIRE(signal_context_set_crypto_provider(context, &openssl_provider) == 0);
    REQUIRE(curve_generate_key_pair(context, &keyPair) == 0);

    signal_buffer* pkBuf = nullptr;
    REQUIRE(ec_public_key_serialize(&pkBuf, ec_key_pair_get_public(keyPair)) == 0);
    const char* tmp = (const char*)signal_buffer_const_data(pkBuf);
    std::string pubKey;
    pubKey.assign(tmp, tmp + signal_buffer_len(pkBuf));
    std::string encrypted;
    std::string ephemeralPubkey;
    std::string iv;
    uint32_t version;
    std::string sender = "1BbCNCwXYZH6rWZJnL6N4n88UxeCccpKNV";
    REQUIRE(0 == SenderUtils::encryptSender(sender, Base64::encode(pubKey), version, iv, ephemeralPubkey, encrypted));

    uint8_t* shared = nullptr;
    int len = 0;
    ec_public_key* ecDestPubkey = nullptr;
    REQUIRE(curve_decode_point(&ecDestPubkey,
                               reinterpret_cast<const uint8_t*>(ephemeralPubkey.data()),
                               ephemeralPubkey.size(),
                               nullptr) == 0);
    len = curve_calculate_agreement(&shared, ecDestPubkey, ec_key_pair_get_private(keyPair));
    REQUIRE(len > 0);
    uint8_t secret[SHA256_DIGEST_LENGTH];
    SHA256(shared, SHA256_DIGEST_LENGTH, secret);
    std::string decrypted = EvpCipher::decrypt(EvpCipher::Algo::AES_256_CBC, std::string(secret, secret + SHA256_DIGEST_LENGTH), iv, encrypted);
    REQUIRE(decrypted == sender);

    if (keyPair) {
        SIGNAL_UNREF(keyPair);
    }
    if (pkBuf) {
        signal_buffer_free(pkBuf);
    }
    if (shared) {
        free(shared);
    }
    if (ecDestPubkey) {
        SIGNAL_UNREF(ecDestPubkey);
    }
    if (context) {
        signal_context_destroy(context);
    }
}

void threadFun() {
    size_t times = 10000;
    while (times > 0) {
        testEncryptSender();
        times--;
    }
}

TEST_CASE("encryptSender")
{
    std::vector<std::thread> threads;
    for (int i = 0; i < 32; i++) {
        threads.emplace_back(threadFun);
    }

    for (auto& t : threads) {
        t.join();
    }
}

TEST_CASE("isClientVersionSupportEncryptSender")
{
    EncryptSenderConfig config;
    config.androidVersion = 100;
    config.iosVersion = 200;

    Device device;
    bcm::ClientVersion *cv = device.mutable_clientversion();
    cv->set_ostype(bcm::ClientVersion_OSType::ClientVersion_OSType_OSTYPE_IOS);
    cv->set_osversion("vos");
    cv->set_phonemodel("ios");
    cv->set_bcmversion("bcmv23");

    cv->set_bcmbuildcode(199);
    REQUIRE(SenderUtils::isClientVersionSupportEncryptSender(device, config) == false);
    cv->set_bcmbuildcode(200);
    REQUIRE(SenderUtils::isClientVersionSupportEncryptSender(device, config) == true);
    cv->set_bcmbuildcode(201);
    REQUIRE(SenderUtils::isClientVersionSupportEncryptSender(device, config) == true);

    cv->set_ostype(bcm::ClientVersion_OSType::ClientVersion_OSType_OSTYPE_ANDROID);

    cv->set_bcmbuildcode(99);
    REQUIRE(SenderUtils::isClientVersionSupportEncryptSender(device, config) == false);
    cv->set_bcmbuildcode(100);
    REQUIRE(SenderUtils::isClientVersionSupportEncryptSender(device, config) == true);
    cv->set_bcmbuildcode(101);
    REQUIRE(SenderUtils::isClientVersionSupportEncryptSender(device, config) == true);

    cv->set_ostype(bcm::ClientVersion_OSType::ClientVersion_OSType_OSTYPE_UNKNOWN);
    REQUIRE(SenderUtils::isClientVersionSupportEncryptSender(device, config) == false);
}


