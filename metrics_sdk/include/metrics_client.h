#pragma once

#include "concurrent_queue.h"
#include "metrics_config.h"
#include <thread>
#include <assert.h>
#include <sys/time.h>
#include <memory>

namespace bcm {
namespace metrics {

class MetricsStatistic;
class ReportMetrics;
class CounterMetrics;
class MetricsFileOutput;

// =========================
// class for metrics client
// usage:
//   MetricsClient::Init(config)
//   MetricsClient::Instance()->markMicrosecondAndRetCode()...
// =========================
class MetricsClient
{
public:
    MetricsClient(const MetricsConfig& config);

    ~MetricsClient() = default;

    static MetricsClient* Instance()
    {
        // not metrics_assert because:
        // 1) not exposed much .h file (metrics_assert.h -> metrics_log_utils.h -> xxx)
        // 2) there is not configuration condition, should be known at debug version
        assert(kInstance != nullptr);
        return kInstance;
    }

    static void Init(const MetricsConfig& config)
    {
        kInstance = new MetricsClient(config);
        kInstance->start();
    }

    void start();

public:
    // mix metrics
    void markMicrosecondAndRetCode(const std::string& serviceName, const std::string& topic, int64_t duration, int retcode);
    void markMicrosecondAndRetCode(const std::string& serviceName, const std::string& topic, int64_t duration, const std::string& retcode);
    // counter metrics
    void counterSet(const std::string& counterName, int64_t count);
    void counterAdd(const std::string& counterName, int64_t add);
    // direct output metrics
    void directOutput(const std::string& metricsName, const std::string& value);
private:
    // run by thread
    void handleMetricsReport();
    // run by thread
    void resetMetricsStatistic();
    // run by thread
    void outputMetrics();

private:
    /* internal progress block */
    // does not use const because will std::move inner function
    void handleMixReportMetrics(ReportMetrics& reportMetrics);
    void handleCounterReportMetrics(ReportMetrics& reportMetrics);
    void handleDirectOutputReportMetrics(ReportMetrics& reportMetrics);
    void doSetCounter(ReportMetrics& reportMetrics, CounterMetrics& counterMetrics);
    void printDropReportMetrics();

private:
    MetricsConfig m_metricsConfig;
    ConcurrentQueue<ReportMetrics> m_reportMetricsQueue;
    ConcurrentQueue<MetricsStatistic*> m_metricsStatisticQueue;
    std::shared_ptr<MetricsFileOutput> m_metricsFileOutput;
    std::thread m_reportMetricsHandlerThread;
    std::thread m_resetMetricsStatisticThread;
    std::thread m_outputMetricsThread;
    MetricsStatistic* m_metricsStatistic;
    std::mutex m_metricsStatisticMutex;

private:
    static MetricsClient* kInstance;
};


// ==============================
// class for easy metrics marker
// ==============================
class ExecTimeAndReturnCodeMarker {
    std::string m_serviceName;
    std::string m_topic;
    int64_t m_startTime;
    int m_returnCode;

public:
    ExecTimeAndReturnCodeMarker(const std::string& serviceName,
                                const std::string& topic)
            : m_serviceName(serviceName), m_topic(topic)
            , m_startTime(currentMicrosecs()), m_returnCode(0)  {}

    ~ExecTimeAndReturnCodeMarker()
    {
        int64_t duration = currentMicrosecs() - m_startTime;
        MetricsClient::Instance()->markMicrosecondAndRetCode(m_serviceName,
                                                             m_topic,
                                                             duration,
                                                             m_returnCode);
    }

    void setReturnCode(int rc)
    {
        m_returnCode = rc;
    }

    static uint64_t currentMicrosecs()
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return ( (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec );
    }
};


} //namespace
}