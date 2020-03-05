#include <string>
#include <map>
#include <boost/beast/http.hpp>
#include "custom_http_status.h"

namespace bcm {
namespace http = boost::beast::http;

std::string obsoleteReason(unsigned status)
{
    const static std::map<unsigned, std::string> kObsoleteReasons = {
        {static_cast<unsigned>(custom_http_status::limiter_rejected), "been throttled"},
        {static_cast<unsigned>(custom_http_status::upgrade_requried), "upgrade required"}
    };

    http::status s = http::int_to_status(status);
    if (s != http::status::unknown) {
        return http::obsolete_reason(s).to_string();
    }
    auto it = kObsoleteReasons.find(status);
    if (it != kObsoleteReasons.end()) {
        return it->second;
    }
    return "";
}

}
