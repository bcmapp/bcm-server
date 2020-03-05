#include "direct_output_metrics.h"

namespace bcm {
namespace metrics {

DirectOutputMetrics::DirectOutputMetrics()
    : m_metricsName("")
    , m_currentTimestampInMs(0)
    , m_metrics()
{
}

void DirectOutputMetrics::mark(std::string&& value)
{
    m_metrics.emplace_back(std::move(value));
}


void DirectOutputMetrics::getMetricsOutput(std::vector<std::string>& outputs)
{
    for (auto& s : m_metrics) {
        std::string result = m_metricsName + "," +
                             std::to_string(m_currentTimestampInMs) + "," + s;
        outputs.emplace_back(std::move(result));
    }
}

} //namespace
}