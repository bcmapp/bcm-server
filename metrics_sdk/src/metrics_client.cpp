#include "../include/metrics_client.h"
#include "metrics_utils.h"
#include "metrics_types/mix_metrics.h"
#include "metrics_types/counter_metrics.h"
#include "metrics_types/direct_output_metrics.h"
#include "metrics_statistic.h"
#include "report_metrics.h"
#include "metrics_log_utils.h"
#include "metrics_file_output.h"
#include "../include/metrcis_common.h"
#include <string>
#include <map>
#include <sstream>
#include <vector>
#include "report_metrics.h"
#include <condition_variable>
#include <stdio.h>
#include <vector>
#include <inttypes.h>
#include <chrono>

namespace bcm {
namespace metrics {

MetricsClient* MetricsClient::kInstance = nullptr;

MetricsClient::MetricsClient(const MetricsConfig& config)
    : m_metricsConfig(config)
    , m_reportMetricsQueue(config.reportQueueSize)
    , m_metricsStatisticQueue()
    , m_metricsFileOutput(std::make_shared<MetricsFileOutput>(config.metricsDir, config.metricsFileSizeInBytes,
                          config.metricsFileCount, config.clientId, config.writeThresholdInBytes))
    , m_metricsStatistic(new MetricsStatistic())
{
}

void MetricsClient::start()
{
    // start basic metric handler thread
    m_reportMetricsHandlerThread = std::thread(&MetricsClient::handleMetricsReport, this);
    m_reportMetricsHandlerThread.detach();

    // start reset timer
    m_resetMetricsStatisticThread = std::thread(&MetricsClient::resetMetricsStatistic, this);
    m_resetMetricsStatisticThread.detach();

    m_outputMetricsThread = std::thread(&MetricsClient::outputMetrics, this);
    m_outputMetricsThread.detach();
}

void MetricsClient::markMicrosecondAndRetCode(const std::string& serviceName, const std::string& topic, int64_t duration, int retcode)
{
    markMicrosecondAndRetCode(serviceName, topic, duration, std::to_string(retcode));
}

void MetricsClient::markMicrosecondAndRetCode(const std::string& serviceName, const std::string& topic, int64_t duration, const std::string& retcode)
{
    ReportMetrics reportMetrics;
    reportMetrics.m_type = ReportMetricsType::MIX_METRICS;
    // m_string1 ==> m_serviceName
    // m_string2 ==> m_topic
    // m_string3 ==> m_retCode
    // m_int64t1 ==> duration
    reportMetrics.m_string1 = serviceName;
    reportMetrics.m_string2 = topic;
    reportMetrics.m_string3 = retcode;
    reportMetrics.m_int64t1 = duration;
    if (!m_reportMetricsQueue.tryEnqueue(std::move(reportMetrics))) {
        printDropReportMetrics();
    }

}

void MetricsClient::counterSet(const std::string& counterName, int64_t count)
{
    ReportMetrics reportMetrics;
    // m_string1 = counterName
    // m_int64t1 = operation(add/set)
    // m_int64t2 = value
    reportMetrics.m_type = ReportMetricsType::COUNTER_METRICS;
    reportMetrics.m_string1 = counterName;
    reportMetrics.m_int64t1 = kReportMetricsCounterSet;
    reportMetrics.m_int64t2 = count;
    if (!m_reportMetricsQueue.tryEnqueue(std::move(reportMetrics))) {
        printDropReportMetrics();
    }
}

void MetricsClient::counterAdd(const std::string& counterName, int64_t add)
{
    ReportMetrics reportMetrics;
    // m_string1 = counterName
    // m_int64t1 = operation(add/set)
    // m_int64t2 = value
    reportMetrics.m_type = ReportMetricsType::COUNTER_METRICS;
    reportMetrics.m_string1 = counterName;
    reportMetrics.m_int64t1 = kReportMetricsCounterAdd;
    reportMetrics.m_int64t2 = add;
    if (!m_reportMetricsQueue.tryEnqueue(std::move(reportMetrics))) {
        printDropReportMetrics();
    }
}

void MetricsClient::directOutput(const std::string& metricsName, const std::string& value)
{
    ReportMetrics reportMetrics;
    reportMetrics.m_type = ReportMetricsType::DIRECT_OUTPUT_METRICS;
    // m_string1 = metricsName
    // m_string2 = value
    reportMetrics.m_string1 = metricsName;
    reportMetrics.m_string2 = value;
    if (!m_reportMetricsQueue.tryEnqueue(std::move(reportMetrics))) {
        printDropReportMetrics();
    }
}

void MetricsClient::handleMetricsReport()
{
    std::string threadName = "m.sdk.report";
    setThreadName(threadName);
    METRICS_LOG_INFO("%s started!", threadName.c_str());

    while (true) {
        ReportMetrics reportMetrics;
        m_reportMetricsQueue.blockingPop(reportMetrics);

        if (reportMetrics.m_type == ReportMetricsType::MIX_METRICS) {
            handleMixReportMetrics(reportMetrics);
        } else if (reportMetrics.m_type == ReportMetricsType::COUNTER_METRICS) {
            handleCounterReportMetrics(reportMetrics);
        } else if (reportMetrics.m_type == ReportMetricsType::DIRECT_OUTPUT_METRICS) {
            handleDirectOutputReportMetrics(reportMetrics);
        } else {
            METRICS_LOG_ERR("drop unknown reportMetric type, %d", static_cast<int>(reportMetrics.m_type));
        }
    }
}

void MetricsClient::resetMetricsStatistic()
{
    std::string threadName = "m.sdk.reset";
    setThreadName(threadName);
    METRICS_LOG_INFO("%s started!", threadName.c_str());

    while(true) {
        // sleep first because the 1st metrics statistic had init when client create.
        std::this_thread::sleep_for(std::chrono::milliseconds(m_metricsConfig.reportIntervalInMs));

        {
            std::unique_lock<std::mutex> lk(m_metricsStatisticMutex);
            m_metricsStatisticQueue.enqueue(m_metricsStatistic);
            m_metricsStatistic = new MetricsStatistic();
        }

        METRICS_LOG_TRACE("%s reset MetricsStatistic", threadName.c_str());
    }
}

void MetricsClient::outputMetrics()
{
    std::string threadName = "m.sdk.output";
    setThreadName(threadName);
    METRICS_LOG_INFO("%s started!", threadName.c_str());

    while(true) {
        MetricsStatistic* ms;
        m_metricsStatisticQueue.blockingPop(ms);
        METRICS_LOG_TRACE("pop an OutputMetrics from m_metricsStatisticQueue");

        // output mix metrics
        for (auto& m : ms->m_mixMetricsMap) {
            std::vector<std::string> outputs;
            m.second.getMetricsOutput(outputs);
            m_metricsFileOutput->out(outputs);
        }

        // output counter metrics
        for (auto& m : ms->m_counterMetricsMap) {
            std::vector<std::string> outputs;
            m.second.getMetricsOutput(outputs);
            m_metricsFileOutput->out(outputs);
        }

        // direct output metrics
        for (auto& m : ms->m_directOutputMap) {
            std::vector<std::string> outputs;
            m.second.getMetricsOutput(outputs);
            m_metricsFileOutput->out(outputs);
        }

        m_metricsFileOutput->flush();
        delete(ms);
    }
}

void MetricsClient::handleMixReportMetrics(ReportMetrics& reportMetrics)
{
    std::unique_lock<std::mutex> lk(m_metricsStatisticMutex);
    // m_string1 ==> m_serviceName
    // m_string2 ==> m_topic
    // m_string3 ==> m_retCode
    // m_int64t1 ==> duration
    std::string mixMetricsKey = reportMetrics.m_string1 + "_" + reportMetrics.m_string2;
    auto mixMetricsKeySearch = m_metricsStatistic->m_mixMetricsMap.find(mixMetricsKey);
    if (mixMetricsKeySearch == m_metricsStatistic->m_mixMetricsMap.end()) {
        MixMetrics mixMetrics;
        mixMetrics.m_serviceName = reportMetrics.m_string1;
        mixMetrics.m_topic = reportMetrics.m_string2;
        mixMetrics.markDuration(reportMetrics.m_int64t1);
        mixMetrics.m_appVersion = m_metricsConfig.appVersion;
        mixMetrics.m_retCodeMap.emplace(reportMetrics.m_string3, (uint64_t)1);
        mixMetrics.m_currentTimestampInMs = m_metricsStatistic->m_currentTimestampInMs;
        m_metricsStatistic->m_mixMetricsMap.emplace(mixMetricsKey, mixMetrics);
    } else {
        mixMetricsKeySearch->second.markDuration(reportMetrics.m_int64t1);
        auto retCodeSearch = mixMetricsKeySearch->second.m_retCodeMap.find(reportMetrics.m_string3);
        if (retCodeSearch == mixMetricsKeySearch->second.m_retCodeMap.end()) {
            mixMetricsKeySearch->second.m_retCodeMap.emplace(reportMetrics.m_string3, (uint64_t)1);
        } else {
            retCodeSearch->second++;
        }
    }
}

void MetricsClient::handleCounterReportMetrics(ReportMetrics& reportMetrics)
{
    std::unique_lock<std::mutex> lk(m_metricsStatisticMutex);
    // m_string1 = counter_name
    // m_int64t1 = operation(add/set)
    // m_int64t2 = value
    auto counterSearch = m_metricsStatistic->m_counterMetricsMap.find(reportMetrics.m_string1);
    if (counterSearch == m_metricsStatistic->m_counterMetricsMap.end()) {
        CounterMetrics counterMetrics;
        counterMetrics.m_counterName = reportMetrics.m_string1;
        counterMetrics.m_currentTimestampInMs = m_metricsStatistic->m_currentTimestampInMs;
        doSetCounter(reportMetrics, counterMetrics);
        m_metricsStatistic->m_counterMetricsMap.emplace(reportMetrics.m_string1, counterMetrics);
    } else {
        doSetCounter(reportMetrics, counterSearch->second);
    }
}

void MetricsClient::handleDirectOutputReportMetrics(ReportMetrics& reportMetrics)
{
    std::unique_lock<std::mutex> lk(m_metricsStatisticMutex);
    // m_string1 = metricsName
    // m_string2 = value
    auto directOutputSearch = m_metricsStatistic->m_directOutputMap.find(reportMetrics.m_string1);
    if (directOutputSearch == m_metricsStatistic->m_directOutputMap.end()) {
        DirectOutputMetrics directOutputMetrics;
        directOutputMetrics.m_metricsName = reportMetrics.m_string1;
        directOutputMetrics.m_currentTimestampInMs = m_metricsStatistic->m_currentTimestampInMs;
        directOutputMetrics.mark(std::move(reportMetrics.m_string2));
        m_metricsStatistic->m_directOutputMap.emplace(reportMetrics.m_string1, directOutputMetrics);
    } else {
        directOutputSearch->second.mark(std::move(reportMetrics.m_string2));
    }
}


void MetricsClient::doSetCounter(ReportMetrics& reportMetrics, CounterMetrics& counterMetrics)
{
    if (reportMetrics.m_int64t1 == kReportMetricsCounterAdd) {
        counterMetrics.add(reportMetrics.m_int64t2);
    } else if (reportMetrics.m_int64t1 == kReportMetricsCounterSet) {
        counterMetrics.set(reportMetrics.m_int64t2);
    } else {
        METRICS_LOG_ERR("unknown CounterMetrics type: %" PRId64, reportMetrics.m_int64t1);
    }
}

void MetricsClient::printDropReportMetrics()
{
    static thread_local int64_t lastPrintTime = 0;
    static thread_local uint64_t lastDropCount = 0;
    int64_t now = MetricsCommon::nowInMilli();
    lastDropCount++;
    // print once 1000ms
    if (now - lastPrintTime >= 1000) {
        METRICS_LOG_INFO("drop report metrics, drop: %" PRId64 " after previous log in this thread", lastDropCount);
        lastPrintTime = now;
        lastDropCount = 0;
    }
}

} //namespace
}

