#include <utility>

#pragma once

#include <boost/beast/http.hpp>
#include <utils/log.h>

namespace bcm {

namespace http = boost::beast::http;

class HttpStatics {
public:
    HttpStatics();
    ~HttpStatics();

    void setPrefix(std::string prefix) { m_prefix = std::move(prefix); }
    void setMethod(http::verb method) { m_method = method; }
    void setTarget(std::string target) { m_target = std::move(target); }
    void setUid(std::string uid) { m_uid = std::move(uid); }
    void setStatus(unsigned status) { m_status = status; }
    void setStatus(http::status status) { m_status = static_cast<unsigned>(status); }
    void setReason(std::string reason) { m_reason = std::move(reason); }
    void setMessage(std::string message) { m_message = std::move(message); }
    void setLogLevel(LogSeverity level) { m_logLevel = level; }

    void onStart();
    void onFinish();

private:
    bool m_isStarted{false};
    std::string m_prefix{"http"};
    http::verb m_method{http::verb::unknown};
    std::string m_target;
    std::string m_uid;
    unsigned m_status{static_cast<unsigned>(http::status::unknown)};
    std::string m_reason;
    std::string m_message;
    int64_t m_startTimestamp;
    LogSeverity m_logLevel{LOGSEVERITY_INFO};
};

}