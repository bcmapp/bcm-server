#pragma once

#include <string>

namespace bcm {

class AnyBase {
public:
    static bool encode(uint64_t num, int base, std::string& encoded);
    static bool decode(const std::string& encoded, int base, uint64_t& num);
};

}
