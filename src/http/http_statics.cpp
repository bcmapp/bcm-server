#include <utils/time.h>
#include <boost/beast/http.hpp>
#include "http_statics.h"
#include "custom_http_status.h"

namespace bcm {

HttpStatics::HttpStatics()
{
    m_startTimestamp = nowInMilli();
}

HttpStatics::~HttpStatics()
{
    onFinish();
}

void HttpStatics::onStart()
{
    m_isStarted = true;

    std::string logMsg;
    logMsg += m_prefix;
    logMsg += " start ";
    logMsg +=  http::to_string(m_method).to_string();
    logMsg += "_";
    logMsg += (m_target.empty() ? "request" : m_target);

    if (!m_uid.empty()) {
        logMsg += ", uid: ";
        logMsg += m_uid;
    }

    LOG2(m_logLevel) << logMsg;
}

void HttpStatics::onFinish()
{
    if (!m_isStarted) {
        return;
    }

    auto duration = nowInMilli() - m_startTimestamp;
    std::string logMsg;
    logMsg += m_prefix;
    logMsg += " finish ";
    logMsg +=  http::to_string(m_method).to_string();
    logMsg += "_";
    logMsg += (m_target.empty() ? "request" : m_target);

    if (!m_uid.empty()) {
        logMsg += ", uid: ";
        logMsg += m_uid;
    }

    logMsg += ", duration: ";
    logMsg += std::to_string(duration);
    logMsg += "ms";

    logMsg += ", status: ";
    if (m_status != static_cast<unsigned>(http::status::unknown)) {
        logMsg += bcm::obsoleteReason(m_status);
    } else if (!m_reason.empty()) {
        logMsg += m_reason;
    } else {
        logMsg += "unknown";
    }

    if (!m_message.empty()) {
        logMsg += ", message: ";
        logMsg += m_message;
    }

    LOG2(m_logLevel) << logMsg;
}

}