#include <stdexcept>
#include "hex_encoder.h"

namespace bcm {

std::string HexEncoder::encode(const std::string& raw, bool bLowerCase, const std::string& prefix)
{
    static constexpr char hexmapLower[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    static constexpr char hexmapUpper[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                           '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    const char* hexmap = hexmapLower;
    if (!bLowerCase) {
        hexmap = hexmapUpper;
    }

    std::string hexstr(raw.size() * 2, ' ');
    for (size_t i = 0; i < raw.size(); ++i) {
        hexstr[2 * i] = hexmap[(raw[i] & 0xF0) >> 4];
        hexstr[2 * i + 1] = hexmap[raw[i] & 0x0F];
    }
    return prefix + hexstr;
}

std::string HexEncoder::decode(const std::string& hexstr, bool bException)
{
    std::string raw(hexstr.size() / 2, ' ');
    uint8_t rawByte = 0;

    for (size_t i = 0; i < hexstr.size(); ++i) {
        char c = hexstr.at(i);
        char part;
        if ('0' <= c && c <= '9') {
            part = c - '0';
        } else if ('a' <= c && c <= 'f') {
            part = c - 'a' + (char)0x0A;
        } else if ('A' <= c && c <= 'F') {
            part = c - 'A' + (char)0x0A;
        } else {
            if (bException) {
                throw std::string(hexstr + " is not a hex string");
            } else {
                return "";
            }
        }
        rawByte = (rawByte << 4) | part;
        if (i & 0x01) {
            raw[i / 2] = rawByte;
            rawByte = 0;
        }
    }

    return raw;
}

}