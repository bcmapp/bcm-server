#include "sender_utils.h"
#include "log.h"
#include <openssl/opensslv.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <signal/curve.h>
#include <openssl/evp.h>
#include <boost/beast/core/detail/base64.hpp>
#include <crypto/evp_cipher.h>
#include <crypto/base64.h>

namespace bcm {

static int openssl_random_generator(uint8_t* data, size_t len, void*)
{
    if (RAND_bytes(data, len)) {
        return 0;
    } else {
        return SG_ERR_UNKNOWN;
    }
}

static int openssl_hmac_sha256_init(void** hmac_context, const uint8_t* key, size_t key_len, void*)
{
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
    HMAC_CTX *ctx = HMAC_CTX_new();
    if(!ctx) {
        return SG_ERR_NOMEM;
    }
#else
    HMAC_CTX* ctx = static_cast<HMAC_CTX*>(malloc(sizeof(HMAC_CTX)));
    if (!ctx) {
        return SG_ERR_NOMEM;
    }
    HMAC_CTX_init(ctx);
#endif

    *hmac_context = ctx;

    if (HMAC_Init_ex(ctx, key, key_len, EVP_sha256(), 0) != 1) {
        return SG_ERR_UNKNOWN;
    }

    return 0;
}

static int openssl_hmac_sha256_update(void* hmac_context, const uint8_t* data, size_t data_len, void*)
{
    HMAC_CTX* ctx = static_cast<HMAC_CTX*>(hmac_context);
    int result = HMAC_Update(ctx, data, data_len);
    return (result == 1) ? 0 : -1;
}

static int openssl_hmac_sha256_final(void* hmac_context, signal_buffer** output, void*)
{
    int result = 0;
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC_CTX* ctx = static_cast<HMAC_CTX*>(hmac_context);

    if (HMAC_Final(ctx, md, &len) != 1) {
        return SG_ERR_UNKNOWN;
    }

    signal_buffer* output_buffer = signal_buffer_create(md, len);
    if (!output_buffer) {
        return SG_ERR_NOMEM;
    }

    *output = output_buffer;

    return result;
}

static void openssl_hmac_sha256_cleanup(void* hmac_context, void*)
{
    if (hmac_context) {
        HMAC_CTX* ctx = static_cast<HMAC_CTX*>(hmac_context);
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
        HMAC_CTX_free(ctx);
#else
        HMAC_CTX_cleanup(ctx);
        free(ctx);
#endif
    }
}

static int openssl_sha512_digest_init(void** digest_context, void*)
{
    int result = 0;
    EVP_MD_CTX* ctx;

    do {
        ctx = EVP_MD_CTX_create();
        if (!ctx) {
            result = SG_ERR_NOMEM;
            break;
        }

        result = EVP_DigestInit_ex(ctx, EVP_sha512(), 0);
        if (result == 1) {
            result = SG_SUCCESS;
        } else {
            result = SG_ERR_UNKNOWN;
        }
    } while (0);

    if (result < 0) {
        if (ctx) {
            EVP_MD_CTX_destroy(ctx);
        }
    } else {
        *digest_context = ctx;
    }
    return result;
}

static int openssl_sha512_digest_update(void* digest_context, const uint8_t* data, size_t data_len, void*)
{
    EVP_MD_CTX* ctx = static_cast<EVP_MD_CTX*>(digest_context);

    int result = EVP_DigestUpdate(ctx, data, data_len);

    return (result == 1) ? SG_SUCCESS : SG_ERR_UNKNOWN;
}

static int openssl_sha512_digest_final(void* digest_context, signal_buffer** output, void*)
{
    int result = 0;
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    EVP_MD_CTX* ctx = static_cast<EVP_MD_CTX*>(digest_context);

    do {
        result = EVP_DigestFinal_ex(ctx, md, &len);
        if (result == 1) {
            result = SG_SUCCESS;
        } else {
            result = SG_ERR_UNKNOWN;
            break;
        }

        result = EVP_DigestInit_ex(ctx, EVP_sha512(), 0);
        if (result == 1) {
            result = SG_SUCCESS;
        } else {
            result = SG_ERR_UNKNOWN;
            break;
        }

        signal_buffer* output_buffer = signal_buffer_create(md, len);
        if (!output_buffer) {
            result = SG_ERR_NOMEM;
            break;
        }

        *output = output_buffer;
    } while (0);

    return result;
}

static void openssl_sha512_digest_cleanup(void* digest_context, void*)
{
    EVP_MD_CTX* ctx = static_cast<EVP_MD_CTX*>(digest_context);
    EVP_MD_CTX_destroy(ctx);
}

static const EVP_CIPHER* openssl_aes_cipher(int cipher, size_t key_len)
{
    if (cipher == SG_CIPHER_AES_CBC_PKCS5) {
        if (key_len == 16) {
            return EVP_aes_128_cbc();
        } else if (key_len == 24) {
            return EVP_aes_192_cbc();
        } else if (key_len == 32) {
            return EVP_aes_256_cbc();
        }
    } else if (cipher == SG_CIPHER_AES_CTR_NOPADDING) {
        if (key_len == 16) {
            return EVP_aes_128_ctr();
        } else if (key_len == 24) {
            return EVP_aes_192_ctr();
        } else if (key_len == 32) {
            return EVP_aes_256_ctr();
        }
    }
    return 0;
}

static int openssl_encrypt(signal_buffer** output,
                           int cipher,
                           const uint8_t* key, size_t key_len,
                           const uint8_t* iv, size_t iv_len,
                           const uint8_t* plaintext, size_t plaintext_len,
                           void*)
{
    int result = 0;
    EVP_CIPHER_CTX* ctx = 0;
    uint8_t* out_buf = 0;

    do {
        const EVP_CIPHER* evp_cipher = openssl_aes_cipher(cipher, key_len);
        if (!evp_cipher) {
            LOGE << "invalid AES mode or key size: " << key_len;
            return SG_ERR_UNKNOWN;
        }

        if (iv_len != 16) {
            LOGE << "invalid AES IV size: " << iv_len;
            return SG_ERR_UNKNOWN;
        }

        if (static_cast<int>(plaintext_len) > INT_MAX - EVP_CIPHER_block_size(evp_cipher)) {
            LOGE << "invalid plaintext length: " << plaintext_len;
            return SG_ERR_UNKNOWN;
        }

#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
        ctx = EVP_CIPHER_CTX_new();
        if(!ctx) {
            result = SG_ERR_NOMEM;
            break;
        }
#else
        ctx = static_cast<EVP_CIPHER_CTX*>(malloc(sizeof(EVP_CIPHER_CTX)));
        if (!ctx) {
            result = SG_ERR_NOMEM;
            break;
        }
        EVP_CIPHER_CTX_init(ctx);
#endif

        result = EVP_EncryptInit_ex(ctx, evp_cipher, 0, key, iv);
        if (!result) {
            LOGE << "cannot initialize cipher";
            result = SG_ERR_UNKNOWN;
            break;
        }

        if (cipher == SG_CIPHER_AES_CTR_NOPADDING) {
            result = EVP_CIPHER_CTX_set_padding(ctx, 0);
            if (!result) {
                LOGE << "cannot set padding";
                result = SG_ERR_UNKNOWN;
                break;
            }
        }

        out_buf = static_cast<uint8_t*>(malloc(sizeof(uint8_t) * (plaintext_len + EVP_CIPHER_block_size(evp_cipher))));
        if (!out_buf) {
            LOGE << "cannot allocate output buffer";
            result = SG_ERR_NOMEM;
            break;
        }

        int out_len = 0;
        result = EVP_EncryptUpdate(ctx, out_buf, &out_len, plaintext, plaintext_len);
        if (!result) {
            LOGE << "cannot encrypt plaintext";
            result = SG_ERR_UNKNOWN;
            break;
        }

        int final_len = 0;
        result = EVP_EncryptFinal_ex(ctx, out_buf + out_len, &final_len);
        if (!result) {
            LOGE << "cannot finish encrypting plaintext";
            result = SG_ERR_UNKNOWN;
            break;
        }

        *output = signal_buffer_create(out_buf, out_len + final_len);

    } while (0);

    if (ctx) {
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
        EVP_CIPHER_CTX_free(ctx);
#else
        EVP_CIPHER_CTX_cleanup(ctx);
        free(ctx);
#endif
    }
    if (out_buf) {
        free(out_buf);
    }
    return result;
}

static int openssl_decrypt(signal_buffer** output,
                           int cipher,
                           const uint8_t* key, size_t key_len,
                           const uint8_t* iv, size_t iv_len,
                           const uint8_t* ciphertext, size_t ciphertext_len,
                           void*)
{
    int result = 0;
    EVP_CIPHER_CTX* ctx = 0;
    uint8_t* out_buf = 0;

    do {
        const EVP_CIPHER* evp_cipher = openssl_aes_cipher(cipher, key_len);
        if (!evp_cipher) {
            LOGE << "invalid AES mode or key size: " << key_len;
            return SG_ERR_INVAL;
        }

        if (iv_len != 16) {
            LOGE << "invalid AES IV size: " << iv_len;
            return SG_ERR_INVAL;
        }

        if (static_cast<int>(ciphertext_len) > INT_MAX - EVP_CIPHER_block_size(evp_cipher)) {
            LOGE << "invalid ciphertext length: " << ciphertext_len;
            return SG_ERR_UNKNOWN;
        }

#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
        ctx = EVP_CIPHER_CTX_new();
        if(!ctx) {
            result = SG_ERR_NOMEM;
            break;
        }
#else
        ctx = static_cast<EVP_CIPHER_CTX*>(malloc(sizeof(EVP_CIPHER_CTX)));
        if (!ctx) {
            result = SG_ERR_NOMEM;
            break;
        }
        EVP_CIPHER_CTX_init(ctx);
#endif

        result = EVP_DecryptInit_ex(ctx, evp_cipher, 0, key, iv);
        if (!result) {
            LOGE << "cannot initialize cipher";
            result = SG_ERR_UNKNOWN;
            break;
        }

        if (cipher == SG_CIPHER_AES_CTR_NOPADDING) {
            result = EVP_CIPHER_CTX_set_padding(ctx, 0);
            if (!result) {
                LOGE << "cannot set padding";
                result = SG_ERR_UNKNOWN;
                break;
            }
        }

        out_buf = static_cast<uint8_t*>(malloc(sizeof(uint8_t) * (ciphertext_len + EVP_CIPHER_block_size(evp_cipher))));
        if (!out_buf) {
            LOGE << "cannot allocate output buffer";
            result = SG_ERR_UNKNOWN;
            break;
        }

        int out_len = 0;
        result = EVP_DecryptUpdate(ctx,
                                   out_buf, &out_len, ciphertext, ciphertext_len);
        if (!result) {
            LOGE << "cannot decrypt ciphertext";
            result = SG_ERR_UNKNOWN;
            break;
        }

        int final_len = 0;
        result = EVP_DecryptFinal_ex(ctx, out_buf + out_len, &final_len);
        if (!result) {
            LOGE << "cannot finish decrypting ciphertext";
            result = SG_ERR_UNKNOWN;
            break;
        }

        *output = signal_buffer_create(out_buf, out_len + final_len);
    } while (0);

    if (ctx) {
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
        EVP_CIPHER_CTX_free(ctx);
#else
        EVP_CIPHER_CTX_cleanup(ctx);
        free(ctx);
#endif
    }
    if (out_buf) {
        free(out_buf);
    }
    return result;
}

static signal_crypto_provider openssl_provider = {
        .random_func = openssl_random_generator,
        .hmac_sha256_init_func = openssl_hmac_sha256_init,
        .hmac_sha256_update_func = openssl_hmac_sha256_update,
        .hmac_sha256_final_func = openssl_hmac_sha256_final,
        .hmac_sha256_cleanup_func = openssl_hmac_sha256_cleanup,
        .sha512_digest_init_func = openssl_sha512_digest_init,
        .sha512_digest_update_func = openssl_sha512_digest_update,
        .sha512_digest_final_func = openssl_sha512_digest_final,
        .sha512_digest_cleanup_func = openssl_sha512_digest_cleanup,
        .encrypt_func = openssl_encrypt,
        .decrypt_func = openssl_decrypt,
        .user_data = nullptr
};

int SenderUtils::encryptSender(const std::string& sender,
                               const std::string& ecdhDestPubKey,
                               uint32_t& version,
                               std::string& iv,
                               std::string& ephemeralPubkey,
                               std::string& encrypted)
{
    signal_context* context = nullptr;
    int ret = 0;
    if (signalContext.get() == nullptr) {
        // initialize signal context
        if ((ret = signal_context_create(&context, 0)) != 0) {
            LOGE << "signal_context_create error, result code : " << ret;
            return ret;
        }
        if ((ret = signal_context_set_crypto_provider(context, &openssl_provider)) != 0) {
            LOGE << "curve_generate_key_pair error, result code : " << ret;
            SIGNAL_UNREF(context);
            return ret;
        }
        signalContext.reset(context);
    } else {
        context = signalContext.get();
    }
    ec_key_pair* keyPair = nullptr;
    ec_public_key* ecDestPubkey = nullptr;
    uint8_t* shared = nullptr;
    signal_buffer* epkBuf = nullptr;

    do {
        // 1) generate curve 25519 key pair
        if ((ret = curve_generate_key_pair(context, &keyPair)) != 0) {
            LOGE << "signal_context_set_crypto_provider error, result code : " << ret;
            break;
        }

        // 2) ECDH -> P
        // 2.1) get ec_pubkey by ecdhDestPubKey
        std::string decodedKey = Base64::decode(ecdhDestPubKey);
        if ((ret = curve_decode_point(&ecDestPubkey,
                                      reinterpret_cast<const uint8_t*>(decodedKey.data()),
                                      decodedKey.size(),
                                      nullptr)) != 0) {
            LOGE << "curve_decode_point for ecdhDestPubKey failed, result code : " << ret;
            break;
        }

        // 2.2) calculate shared key through ecdh
        int sharedLen = 0;
        if ((sharedLen = curve_calculate_agreement(&shared,
                                                   ecDestPubkey,
                                                   ec_key_pair_get_private(keyPair))) < 0) {
            ret = -1;
            LOGE << "curve_calculate_agreement failed, return sharedLen : " << sharedLen;
            break;
        }

        // secret=sha256(shared)
        uint8_t secret[SHA256_DIGEST_LENGTH];
        SHA256(shared, sharedLen, secret);
        // encrypt
        encrypted = EvpCipher::encrypt(EvpCipher::Algo::AES_256_CBC, std::string(secret, secret + SHA256_DIGEST_LENGTH), iv, sender);
        if (encrypted.empty()) {
            ret = -1;
            LOGE << "encrypt sender failed, encrypted result is empty";
            break;
        }
        if ((ret = ec_public_key_serialize(&epkBuf, ec_key_pair_get_public(keyPair))) != 0) {
            LOGE << "ec_public_key_serialize failed, result code : " << ret;
            break;
        }
        version = 0;
        const char* tmp = (const char*)signal_buffer_const_data(epkBuf);
        ephemeralPubkey.assign(tmp, tmp + signal_buffer_len(epkBuf));

    } while (0);

    if (keyPair) {
        SIGNAL_UNREF(keyPair);
    }

    if (ecDestPubkey) {
        SIGNAL_UNREF(ecDestPubkey);
    }

    //TODO the memory that shared referenced to is allocated by signal protocol library, but there is no deallocate api
    // for it. here directly free shared may cause some issue
    if (shared) {
        free(shared);
    }

    if (epkBuf) {
        signal_buffer_free(epkBuf);
    }

    return ret;
}

bool SenderUtils::isClientVersionSupportEncryptSender(const Device& device,
                                                      const EncryptSenderConfig& encryptSenderConfig)
{
    // destination is old version, can send
    auto& clientVer = device.clientversion();
    if (clientVer.ostype() == ClientVersion::OSTYPE_IOS) {
        if (clientVer.bcmbuildcode() < encryptSenderConfig.iosVersion) {
            return false;
        }
    } else if (clientVer.ostype() == ClientVersion::OSTYPE_ANDROID) {
        if (clientVer.bcmbuildcode() < encryptSenderConfig.androidVersion) {
            return false;
        }
    } else {
        // client report wrong client version,
        // so we consider it's not support
        LOGW << "found wrong client version, version type: " << clientVer.ostype();
        return false;
    }
    return true;
}

std::string SenderUtils::getSourceInPushService(const Device& destinationDevice,
                                                const Envelope& envelope,
                                                const EncryptSenderConfig& encryptSenderConfig)
{
    if ("" == envelope.source()) {
        return envelope.sourceextra();
    }

    if (isClientVersionSupportEncryptSender(destinationDevice, encryptSenderConfig)) {
        return envelope.sourceextra();
    }

    return envelope.source();

}

thread_local SenderUtils::Guard <signal_context> SenderUtils::signalContext(nullptr, [](signal_context* p)
{
    if (p) {
        SIGNAL_UNREF(p);
    }
});

}

