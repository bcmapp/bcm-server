#include "counter_metrics.h"

namespace bcm {
namespace metrics {

CounterMetrics::CounterMetrics()
    : m_counterName("")
    , m_currentTimestampInMs(0)
    , m_value(0)
{
}

void CounterMetrics::add(int64_t value)
{
    m_value += value;
}

void CounterMetrics::set(int64_t value)
{
    m_value = value;
}

void CounterMetrics::getMetricsOutput(std::vector<std::string>& outputs)
{
    std::string result = m_counterName + "," +
                         std::to_string(m_currentTimestampInMs) + "," +
                         std::to_string(m_value);
    outputs.emplace_back(std::move(result));
}

} //namespace
}
