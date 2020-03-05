#pragma once

#include <string>
#include <vector>

namespace bcm {
namespace metrics {

class DirectOutputMetrics
{
public:
    DirectOutputMetrics();
    ~DirectOutputMetrics() = default;

    void mark(std::string&& value);

    void getMetricsOutput(std::vector<std::string>& outputs);

public:
    std::string m_metricsName;
    int64_t m_currentTimestampInMs;

private:
    std::vector<std::string> m_metrics;

};


} //namespace
}