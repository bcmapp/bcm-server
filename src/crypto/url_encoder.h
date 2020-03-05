#pragma once

#include <string>

namespace bcm {

class UrlEncoder {
public:
    static std::string encode(const std::string& url);
    static std::string decode(const std::string& url);
};

}