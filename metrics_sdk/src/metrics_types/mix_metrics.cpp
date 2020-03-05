#include "mix_metrics.h"

namespace bcm {
namespace metrics {

MixMetrics::MixMetrics()
    : m_serviceName("")
    , m_topic("")
    , m_appName("")
    , m_appVersion("")
    , m_currentTimestampInMs(0)
    , m_retCodeMap()
    , m_durations{0}
    , m_durationIndex(0)
    , m_type("mix")
{
}

void MixMetrics::markDuration(int64_t duration)
{
    if (m_durationIndex >= kMaxDurationsSize) {
        return;
    }
    m_durations[m_durationIndex++] = duration;
}

void MixMetrics::getMetricsOutput(std::vector<std::string>& outputs)
{
    int64_t totalDuration = 0;
    int64_t avgDuration = 0;
    if (m_durationIndex != 0) {
        for (int i=0; i<m_durationIndex; ++i) {
            totalDuration += m_durations[i];
        }
        avgDuration = totalDuration / m_durationIndex;
    }

    for (auto& m : m_retCodeMap) {
        std::string result;
        result = m_type + "," + std::to_string(m_currentTimestampInMs) + "," +
                 m_serviceName + "," + m_topic + "," + m_appVersion + "," +
                 std::to_string(m.second) + "," + m.first + "," + std::to_string(avgDuration);
        outputs.emplace_back(std::move(result));
    }
}

} //namespace
}