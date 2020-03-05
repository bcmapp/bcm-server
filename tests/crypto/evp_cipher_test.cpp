#include "../test_common.h"

#include <crypto/evp_cipher.h>
#include <crypto/hex_encoder.h>

using namespace bcm;

TEST_CASE("EvpCipher")
{
    std::string iv = HexEncoder::decode("1c60afbf28029b6b644bf7950bbb4ccf");
    std::string key = "abcdefghijklmnopqrstuvwxyz012345";
    std::string plaintext = "plaintext";
    std::string ciphertext = HexEncoder::decode("aa849a2a21e3e741af7d6a2ed7d6e1df");
    REQUIRE(EvpCipher::encrypt(EvpCipher::Algo::AES_256_CBC, key, iv, plaintext) == ciphertext);
    REQUIRE(EvpCipher::decrypt(EvpCipher::Algo::AES_256_CBC, key, iv, ciphertext) == plaintext);

    iv = HexEncoder::decode("1de863c4560d6e64f1321442f81e9254");
    plaintext = "plaintextplaintextplaintextplaintextplaintextplaintextplaintextplaintexts";
    ciphertext = HexEncoder::decode("121185d153819f5e22ce4aee760ca02cd7c03be52964177f56f9932908466"\
                                    "1ab8b26539ff3b3709676672e267c0ba48918554d9a0affdac51133fc358f"\
                                    "77886b722cc2c9dd332a4ebafdd7e4147d7016");
    REQUIRE(EvpCipher::encrypt(EvpCipher::Algo::AES_256_CBC, key, iv, plaintext) == ciphertext);
    REQUIRE(EvpCipher::decrypt(EvpCipher::Algo::AES_256_CBC, key, iv, ciphertext) == plaintext);
}