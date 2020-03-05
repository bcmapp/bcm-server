#pragma once

#include <string>

namespace bcm {

class SHA1 {
public:
    static std::string digest(const std::string& msg);
};

} // namespace bcm
