#pragma once

#include <boost/asio/ssl.hpp>

namespace bcm {

namespace ssl = boost::asio::ssl;

class SslUtils {
public:
    static std::shared_ptr<ssl::context> loadServerCertificate(const std::string& certPath, const std::string& keyPath,
                                                               const std::string& password);

    static ssl::context& getGlobalClientContext();

};

}