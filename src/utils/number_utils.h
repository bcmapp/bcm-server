#pragma once

#include <string>

namespace bcm {

class NumberUtils {
public:
    static bool isValidNumber(const std::string& number);
    static bool isTestNumber(const std::string& number);
    static std::string getNumberToken(const std::string& number);
    static std::string getBase64NumberToken(const std::string& number);
    static std::string genVerificationCode(const std::string& number);
};

}
