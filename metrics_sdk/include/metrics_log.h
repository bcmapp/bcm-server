#pragma once

#include <functional>

namespace bcm {
namespace metrics {

// =========================
// block metrics log
// =========================
enum MetricsLogLevel {
    METRICS_LOGLEVEL_TRACE = 0,
    METRICS_LOGLEVEL_DEBUG = 1,
    METRICS_LOGLEVEL_INFO = 2,
    METRICS_LOGLEVEL_WARN = 3,
    METRICS_LOGLEVEL_ERROR = 4,
    METRICS_LOGLEVEL_FATAL = 5
};

typedef std::function<void(MetricsLogLevel level, const std::string& str)> MetricsLogFunc;

class MetricsLogger {
public:
    MetricsLogger() : m_metricsLoggerFunc(nullptr)
    {}

    static MetricsLogger* instance()
    {
        static MetricsLogger instance;
        return &instance;
    }

    void registLogFunction(MetricsLogFunc mLog)
    {
        m_metricsLoggerFunc = mLog;
    }

    bool isLog()
    {
        if (m_metricsLoggerFunc == nullptr) {
            return false;
        }
        return true;
    }

    void print(MetricsLogLevel level, const std::string& str)
    {
        m_metricsLoggerFunc(level, str);
    }

private:
    MetricsLogFunc m_metricsLoggerFunc;
};

}  //namespace
}