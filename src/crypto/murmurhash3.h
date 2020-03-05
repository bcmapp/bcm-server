#pragma once

#include <vector>
#include <string>

namespace bcm {

class MurmurHash3 {
    public:
        static uint32_t murmurHash3(uint32_t nHashSeed, const std::vector<uint8_t>& vDataToHash);
        static uint32_t murmurHash3(uint32_t nHashSeed, const std::string& dataToHash);
};

} // namespace bcm
