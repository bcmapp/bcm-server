#include "url_encoder.h"
#include "hex_encoder.h"
#include <sstream>
#include <iomanip>


namespace bcm {

std::string UrlEncoder::encode(const std::string& url)
{
    std::ostringstream out;

    for (char c : url) {
        if ((c >= 'a' && c <= 'z')
            || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9')
            || (c == '-')
            || (c == '_')
            || (c == '.')
            || (c == '~')) {
            out << c;
        } else {
            out << HexEncoder::encode(std::string(1, c), false, "%");
        }
    }

    return out.str();
}

std::string UrlEncoder::decode(const std::string& url)
{
    std::ostringstream out;

    for (size_t i = 0; i < url.size(); ++i) {
        if (url.at(i) == '%' && (i + 2) < url.size()) {
            std::string hexStr = url.substr(i + 1, 2);
            i += 2;
            try {
                out << HexEncoder::decode(hexStr, true);
            } catch (std::exception&){
                out << "%" << hexStr;
            }
        } else {
            out << url.at(i);
        }
    }

    return out.str();
}

}