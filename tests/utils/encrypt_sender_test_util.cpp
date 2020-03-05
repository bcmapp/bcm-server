#include "../test_common.h"
#include <signal/curve.h>
#include <signal/signal_protocol.h>
#include "../../tools/signal_openssl_provider.h"
#include <crypto/base64.h>

using namespace bcm;

uint8_t privKey[] = {
        0xa0, 0x4e, 0x08, 0x47, 0x98,
        0x3d, 0x04, 0xe1, 0xd4, 0xff,
        0x79, 0x68, 0xd2, 0xf6, 0x80,
        0xdc, 0x9e, 0xb9, 0xc2, 0xd5,
        0xca, 0x8c, 0x2e, 0x8a, 0xd1,
        0x3f, 0x06, 0xea, 0x46, 0x02,
        0xd2, 0x4b};

uint8_t pubKey[] = {
        0xc4, 0xc3, 0x25, 0x15, 0xf8,
        0x82, 0x03, 0x42, 0xac, 0x06,
        0x8d, 0xca, 0xc4, 0xd4, 0x15,
        0xde, 0xaa, 0x90, 0x21, 0xbc,
        0xd7, 0x5b, 0xda, 0x3f, 0x97,
        0x67, 0x85, 0x76, 0x70, 0x38,
        0x91, 0x16};

uint8_t iv[16] = {0x00};

TEST_CASE("encryptSender")
{
    signal_context* context = nullptr;
    ec_private_key* ec_private_key = nullptr;
    ec_public_key* ec_public_key = nullptr;
    REQUIRE(signal_context_create(&context, 0) == 0);
    REQUIRE(signal_context_set_crypto_provider(context, &openssl_provider) == 0);
    REQUIRE(curve_decode_private_point(&ec_private_key, privKey, sizeof(privKey), nullptr) == 0);
    REQUIRE(curve_generate_public_key(&ec_public_key, ec_private_key) == 0);

    signal_buffer* pub_key_buffer = nullptr;
    REQUIRE(ec_public_key_serialize(&pub_key_buffer, ec_public_key) == 0);

    signal_buffer* signature = nullptr;
    REQUIRE(curve_calculate_signature(context, &signature, ec_private_key, iv, sizeof(iv)) == 0);
    const char* tmp = (const char*)signal_buffer_const_data(signature);
    std::string sig(tmp, tmp + signal_buffer_len(signature));
    std::cout << "base64 iv:" << Base64::encode(std::string(iv, iv + 16)) << std::endl;
    std::cout << "iv signature: " << Base64::encode(sig) << std::endl;
    tmp = (const char*)signal_buffer_const_data(pub_key_buffer);
    std::cout << "base64 generated public key: " << Base64::encode(std::string(tmp, signal_buffer_len(pub_key_buffer))) << std::endl;
    std::cout << "base64 public key: " << Base64::encode(std::string(pubKey, pubKey + 32)) << std::endl;
}
