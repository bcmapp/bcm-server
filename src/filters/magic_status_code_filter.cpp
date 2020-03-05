#ifndef NDEBUG

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>
#include "magic_status_code_filter.h"
#include "http/custom_http_status.h"
#include "utils/log.h"

namespace bcm {
namespace http = boost::beast::http;

const std::string kMagicStatusCode = "/magic_status_code";

bool MagicStatusCodeFilter::onFilter(const http::request<http::string_body>& header, uint32_t& status, std::string& reason)
{
    std::string target = header.target().to_string();
    target = target.substr(0, target.find('?'));
    if (kMagicStatusCode == target) {
        std::string data = header["data"].to_string();
        nlohmann::json j = nlohmann::json::parse(data, nullptr, false);
        if (j.find("action") == j.end() || j.find("target") == j.end() || j.find("code") == j.end()) {
            status = static_cast<unsigned>(http::status::bad_request);
            reason = bcm::obsoleteReason(status);
            return true;
        }
        std::string action = j["action"].get<std::string>();
        std::string m = j["target"].get<std::string>();
        if (action == "add") {
            unsigned c = j["code"].get<unsigned>();
            m_Codes[m] = c;
            LOGI << "magic status code registered, target: " << m << ", code: " << c;
        } else {
            m_Codes.erase(m);
            LOGI << "magic status code deregistered, target: " << m;
        }
        status = static_cast<unsigned>(http::status::ok);
        reason = bcm::obsoleteReason(status);
        return true;
    }
    auto it = m_Codes.find(target);
    if (it != m_Codes.end()) {
        status = it->second;
        reason = bcm::obsoleteReason(status);
        return true;
    }
    return false;
}

}

#endif
