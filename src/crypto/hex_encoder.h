#pragma once

#include <string>

namespace bcm {

class HexEncoder {
public:
    static std::string encode(const std::string& raw, bool bLowerCase = true, const std::string& prefix = "");
    static std::string decode(const std::string& hexstr, bool bException = false);
};

}

