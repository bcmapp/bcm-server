#pragma once

#include <map>
#include <string>
#include <vector>

namespace bcm {
namespace metrics {

class MixMetrics
{
public:
    MixMetrics();
    ~MixMetrics() = default;

    void markDuration(int64_t duration);

    void getMetricsOutput(std::vector<std::string>& outputs);

public:
    std::string m_serviceName;
    std::string m_topic;
    std::string m_appName;
    std::string m_appVersion;
    int64_t m_currentTimestampInMs;
    // map<retCode, requestTimes>
    std::map<std::string, uint64_t> m_retCodeMap;

private:
    static const int32_t kMaxDurationsSize = 5000;
    int64_t m_durations[kMaxDurationsSize];
    int32_t m_durationIndex;
    std::string m_type;

};


} //namespace
}