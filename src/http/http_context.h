#pragma once

#include <string>
#include <map>
#include <boost/beast.hpp>
#include <boost/any.hpp>
#include "http_statics.h"

namespace bcm {

namespace http = boost::beast::http;

struct HttpContext {
    http::request<http::string_body> request{};
    http::response<http::string_body> response{};

    std::map<std::string, std::string> pathParams{};
    std::map<std::string, std::string> queryParams{};
    boost::any authResult{};
    boost::any requestEntity{};
    boost::any responseEntity{};

    HttpStatics statics;

    HttpContext() = default;
};


}
