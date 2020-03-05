#pragma once

#include <string>
#include <openssl/evp.h>

namespace bcm {

class EvpCipher {
public:
    enum class Mode {
        ENCRYPT,
        DECRYPT,
    };

    enum class Algo {
        AES_128_CBC,
        AES_256_CBC,
    };

    EvpCipher();
    ~EvpCipher();

    int init(Mode mode, Algo algo, const std::string& key, std::string& iv);
    std::string update(const std::string& text);
    std::string final();

    static std::string encrypt(Algo algo, const std::string& key,
                               std::string& iv, const std::string& plaintext);
    static std::string decrypt(Algo algo, const std::string& key,
                               std::string& iv, const std::string& ciphertext);

private:
    static std::string generateIV(uint32_t size);

private:
    EVP_CIPHER_CTX m_ctx;

};

}

