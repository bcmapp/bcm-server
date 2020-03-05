#pragma once

namespace bcm {
namespace metrics {

const static int64_t kReportMetricsCounterAdd = 1;
const static int64_t kReportMetricsCounterSet = 2;

enum class ReportMetricsType
{
    UNKNOWN = 0,
    MIX_METRICS = 1,
    COUNTER_METRICS = 2,
    DIRECT_OUTPUT_METRICS = 3
};

struct ReportMetrics
{
    ReportMetricsType m_type;
    std::string m_string1;
    std::string m_string2;
    std::string m_string3;
    int64_t m_int64t1;
    int64_t m_int64t2;
};


} //namespace
}