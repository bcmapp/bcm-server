#pragma once

#include <string>

namespace bcm {

class Gzip {
public:
    static std::string compress(const std::string& data);
    static std::string decompress(const std::string& data);

};

}