#pragma once
#include <string>
#include <sstream>
#include <boost/beast/http.hpp>

namespace bcm {

namespace http = boost::beast::http;

struct Api {
    Api() {}
    Api(const http::verb& m, const std::string& n) : method(m), name(n) {}
    http::verb method;
    std::string name;
};

static inline bool operator> (const Api& lhs, const Api& rhs)
{
    if (lhs.method > rhs.method) {
        return true;
    } else if (lhs.method == rhs.method) {
        return (lhs.name > rhs.name);
    }
    return false;
}

static inline std::string to_string(const Api& api)
{
    if (api.name.empty()) {
        return "";
    }
    std::ostringstream oss;
    std::string method = http::to_string(api.method).to_string();
    std::for_each(method.begin(), method.end(), [](char & c){
        c = std::tolower(c);
    });
    oss << api.name << "/" << method;
    return oss.str();
}

}
