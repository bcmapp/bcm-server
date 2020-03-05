#pragma once

#include <string>

namespace bcm {

class Base64 {

public:
    static std::string encode(const std::string& target, bool url_safe = false);
    static std::string decode(const std::string& target, bool url_safe = false);
    static bool check(const std::string& target, bool url_safe = false);

};
} // namespace bcm
