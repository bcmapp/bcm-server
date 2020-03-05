#include "ssl_utils.h"
#include <fstream>
#include <iostream>
#include <boost/core/ignore_unused.hpp>
#include <utils/log.h>

namespace bcm {

using namespace boost;

std::shared_ptr<ssl::context> SslUtils::loadServerCertificate(const std::string& certPath, const std::string& keyPath,
                                                              const std::string& password)
{
    auto context = std::make_shared<ssl::context>(ssl::context::sslv23_server);
    system::error_code ec;

    LOGI << "load server certificate: " << certPath << ", key: " << keyPath << ", password: " << password;

    context->set_password_callback([=](size_t maxLength, ssl::context_base::password_purpose purpose) {
        boost::ignore_unused(purpose);
        LOGD << "read password for purpose " << purpose;
        if (password.size() > maxLength) {
            LOGW << "password is too long: " << password.size() << " > " << maxLength;
            return password.substr(0, maxLength);
        }
        return password;
    });

    context->set_options(ssl::context::default_workarounds | ssl::context::no_sslv2);

    context->use_certificate_chain_file(certPath, ec);
    if (ec) {
        LOGE << "use_certificate_file: " << ec.message();
        return {nullptr};
    }
    context->use_private_key_file(keyPath, ssl::context_base::file_format::pem, ec);
    if (ec) {
        LOGE << "use_private_key_file: " << ec.message();
        return {nullptr};
    }

#ifdef SSL_CTX_set_dh_auto
    SSL_CTX_set_dh_auto(context->native_handle(), 1);
#else
    SSL_CTX_set_ecdh_auto(context->native_handle(), 1);
#endif

    return context;
}

ssl::context& SslUtils::getGlobalClientContext()
{
    static ssl::context g_sslc{ssl::context::sslv23_client};
    static bool g_inited{false};
    if (!g_inited) {
        g_inited = true;
        system::error_code ec;
        g_sslc.set_default_verify_paths(ec);
        g_sslc.set_verify_mode(ssl::verify_none, ec);
    }
    return g_sslc;
}

}


