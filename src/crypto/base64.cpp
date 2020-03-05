#include "base64.h"
#include <boost/beast/core/detail/base64.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>

namespace bcm {

std::string Base64::encode(const std::string& target, bool url_safe)
{
    if (target.empty()) {
        return "";
    }

    std::string result = boost::beast::detail::base64_encode(target);

    if (url_safe) {
        boost::algorithm::replace_all(result, "+", "-");
        boost::algorithm::replace_all(result, "/", "_");
        boost::algorithm::trim_if(result, [](const char& it){return it == '=';});
    }

    return result;
}

std::string Base64::decode(const std::string& target, bool url_safe)
{
    std::string realTarget = boost::algorithm::replace_all_copy(target, "\r\n", "");

    if (realTarget.empty()) {
        return "";
    }

    size_t appendSize = 0;
    if (url_safe) {
        boost::algorithm::replace_all(realTarget, "-", "+");
        boost::algorithm::replace_all(realTarget, "_", "/");
        appendSize = realTarget.size() % 4;
        if (appendSize != 0) {
            realTarget.append(appendSize, '=');
        }
    }

    if (appendSize == 0) {
        for (ssize_t i = (realTarget.size() - 1); i >= 0; --i) {
            if (realTarget[i] != '=') {
                break;
            }
            ++appendSize;
        }
    }

    size_t decodeSize = boost::beast::detail::base64::decoded_size(realTarget.size()) - appendSize;

    auto decoded = boost::beast::detail::base64_decode(realTarget);
    if (decodeSize != decoded.size()) {
        return "";
    }

    return decoded;
}

bool Base64::check(const std::string& target, bool url_safe)
{
    if (target.empty()) {
        return true;
    }

    return !(decode(target, url_safe).empty());
}

} // namespace bcm
