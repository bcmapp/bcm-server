#pragma once

#include <string>
#include <openssl/hmac.h>

namespace bcm {

class Hmac {
public:
    enum class Algo {
        SHA1,
        SHA256,
        SHA512,
        MD5,
    };

    Hmac(Algo algo, const std::string& key);
    ~Hmac();

    Hmac& reset(Algo algo, const std::string& key);
    Hmac& update(const std::string& data);
    std::string final();

    static std::string digest(Algo algo, const std::string& key, const std::string& data);

private:
    HMAC_CTX m_ctx;

};

}



