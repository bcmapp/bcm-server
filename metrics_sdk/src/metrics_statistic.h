#pragma once

#include "metrics_types/mix_metrics.h"
#include "metrics_types/counter_metrics.h"
#include "metrics_types/direct_output_metrics.h"
#include <map>

namespace bcm {
namespace metrics {

class MetricsStatistic
{

public:
    MetricsStatistic();
    ~MetricsStatistic() = default;

public:
    int64_t m_currentTimestampInMs;
    std::map<std::string, MixMetrics> m_mixMetricsMap;
    std::map<std::string, CounterMetrics> m_counterMetricsMap;
    std::map<std::string, DirectOutputMetrics> m_directOutputMap;

};



} //namespace
}