#pragma once

#include <string>
#include <vector>

namespace bcm {
namespace metrics {

class CounterMetrics
{
public:
    CounterMetrics();
    ~CounterMetrics() = default;

    void add(int64_t value);
    void set(int64_t value);

    void getMetricsOutput(std::vector<std::string>& outputs);

public:
    std::string m_counterName;
    int64_t m_currentTimestampInMs;

private:
    int64_t m_value;

};


} //namespace
}