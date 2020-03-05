#pragma once

#include <string>

namespace bcm {

class FNV {
public:
    static uint32_t hash(const char *pKey, size_t ulen);
};

} // namespace bcm