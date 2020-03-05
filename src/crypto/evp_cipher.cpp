#include "evp_cipher.h"
#include <utils/log.h>
#include <crypto/random.h>

namespace bcm {

#define EVP_MODE(opmode) ((mode == EvpCipher::Mode::ENCRYPT) ? 1 : 0)

EvpCipher::EvpCipher()
{
    EVP_CIPHER_CTX_init(&m_ctx);
}

EvpCipher::~EvpCipher()
{
    EVP_CIPHER_CTX_cleanup(&m_ctx);
}

int EvpCipher::init(Mode mode, Algo algo, const std::string& key, std::string& iv)
{
    const EVP_CIPHER* cipher = nullptr;
    switch (algo) {
        case Algo::AES_128_CBC:
            cipher = EVP_aes_128_cbc();
            break;
        case Algo::AES_256_CBC:
            cipher = EVP_aes_256_cbc();
            break;
    }

    if (EVP_CipherInit_ex(&m_ctx, cipher, nullptr, nullptr, nullptr, EVP_MODE(mode)) != 1) {
        LOGW << "init failed!";
        return -1;
    }

    if (static_cast<int>(key.size()) < EVP_CIPHER_CTX_key_length(&m_ctx)) {
        LOGW << "key size is shorter than " << EVP_CIPHER_CTX_key_length(&m_ctx);
        return -1;
    }

    if (!iv.empty()) {
        if (static_cast<int>(iv.size()) < EVP_CIPHER_CTX_iv_length(&m_ctx)) {
            LOGW << "iv size is shorter than " << EVP_CIPHER_CTX_iv_length(&m_ctx);
            return -1;
        }
    } else {
        if (mode == Mode::DECRYPT) {
            LOGW << "iv cannot be empty in decrypt mode";
            return -1;
        }
        iv = generateIV(static_cast<uint32_t>(EVP_CIPHER_CTX_iv_length(&m_ctx)));
    }

    if (EVP_CipherInit_ex(&m_ctx, nullptr, nullptr,
                          reinterpret_cast<const unsigned char*>(key.data()),
                          reinterpret_cast<const unsigned char*>(iv.data()),
                          EVP_MODE(mode)) != 1) {
        LOGW << "init failed!";
        return -1;
    }

    return 0;
}

std::string EvpCipher::generateIV(uint32_t size)
{
    std::string iv;
    while (size > 0) {
        uint64_t random = SecureRandom<uint64_t>::next();
        if (size > sizeof(random)) {
            iv.append(reinterpret_cast<char*>(&random), sizeof(random));
        } else {
            iv.append(reinterpret_cast<char*>(&random), size);
            break;
        }
        size -= sizeof(random);
    }

    return iv;
}

std::string EvpCipher::update(const std::string& text)
{
    std::unique_ptr<char[]> outBuf(new char[text.size() + EVP_MAX_BLOCK_LENGTH]);
    size_t outSize = 0;
    int ret = EVP_CipherUpdate(&m_ctx,
                               reinterpret_cast<unsigned char*>(outBuf.get()),
                               reinterpret_cast<int*>(&outSize),
                               reinterpret_cast<const unsigned char*>(text.data()),
                               static_cast<int>(text.size()));
    if (ret != 1) {
        LOGW << "update failed!";
        return "";
    }
    return std::string(outBuf.get(), outSize);
}

std::string EvpCipher::final()
{
    char outBuf[EVP_MAX_BLOCK_LENGTH];
    size_t outSize = 0;
    int ret = EVP_CipherFinal_ex(&m_ctx, reinterpret_cast<unsigned char*>(outBuf), reinterpret_cast<int*>(&outSize));
    if (ret != 1) {
        LOGW << "final failed!";
        return "";
    }
    return std::string(outBuf, outSize);
}

std::string EvpCipher::encrypt(Algo algo, const std::string& key,
                               std::string& iv, const std::string& plaintext)
{
    std::string out;
    EvpCipher cipher;
    cipher.init(Mode::ENCRYPT, algo, key, iv);
    out.append(cipher.update(plaintext));
    out.append(cipher.final());
    return out;
}

std::string EvpCipher::decrypt(Algo algo, const std::string& key,
                               std::string& iv, const std::string& ciphertext)
{
    std::string out;
    EvpCipher cipher;
    cipher.init(Mode::DECRYPT, algo, key, iv);
    out.append(cipher.update(ciphertext));
    out.append(cipher.final());
    return out;
}

}