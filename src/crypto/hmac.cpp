#include "hmac.h"
#include <utils/log.h>

namespace bcm {

Hmac::Hmac(Algo algo, const std::string& key)
{
    reset(algo, key);
}

Hmac::~Hmac()
{
    HMAC_CTX_cleanup(&m_ctx);
}

Hmac& Hmac::reset(Algo algo, const std::string& key)
{
    const EVP_MD* md = nullptr;
    switch (algo) {
        case Algo::SHA1:
            md = EVP_sha1();
            break;
        case Algo::SHA256:
            md = EVP_sha256();
            break;
        case Algo::SHA512:
            md = EVP_sha512();
            break;
        case Algo::MD5:
            md = EVP_md5();
            break;
    }

    HMAC_CTX_init(&m_ctx);
    int ret = HMAC_Init_ex(&m_ctx, key.data(), static_cast<int>(key.size()), md, nullptr);
    if (ret != 1) {
        LOGE << "hmac init failed, algo: " << static_cast<std::underlying_type<Algo>::type>(algo)
             << " , key size: " << key.size();
    }

    return *this;
}

Hmac& Hmac::update(const std::string& data)
{
    int ret = HMAC_Update(&m_ctx, reinterpret_cast<const unsigned char*>(data.data()), data.size());
    if (ret != 1) {
        LOGE << "hmac update failed";
    }
    return *this;
}

std::string Hmac::final()
{
    char digest[EVP_MAX_MD_SIZE] = {0};
    unsigned int digestSize = 0;

    int ret = HMAC_Final(&m_ctx, reinterpret_cast<unsigned char*>(digest), &digestSize);
    if (ret != 1) {
        LOGE << "hmac final failed";
        return "";
    }

    return std::string(digest, digestSize);
}

std::string Hmac::digest(Algo algo, const std::string& key, const std::string& data)
{
    return Hmac(algo, key).update(data).final();
}

}