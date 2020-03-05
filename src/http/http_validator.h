#pragma once
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>


namespace bcm {

namespace http = boost::beast::http;

class IValidator {
public:
    struct ValidateInfo {
        boost::asio::ip::tcp::endpoint remoteEndpoint;
        const http::request<http::string_body>* request;
        std::string origin;
    };
public:
    virtual bool validate(const ValidateInfo& info, uint32_t& status) = 0;
};

}
