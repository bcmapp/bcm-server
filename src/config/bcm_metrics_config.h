#pragma once

#include <utils/jsonable.h>
#include <metrics_config.h>

namespace bcm
{

struct BCMMetricsConfig
{
    // app version
    std::string appVersion;
    // report queue size, which for collect business thread report metrics
    uint32_t reportQueueSize;
    // metrics files dir
    std::string metricsDir;
    // metrics file size
    uint64_t metricsFileSizeInBytes;
    // metrics file count
    uint32_t metricsFileCount;
    // output metrics interval
    int64_t reportIntervalInMs;
    // metrics client id
    std::string clientId;
    // metrics output Threshold
    int64_t writeThresholdInBytes;

public:
    static void copyToMetricsConfig(const BCMMetricsConfig& bcmMetricsConfig,
                                    metrics::MetricsConfig& metricsConfig)
    {
        metricsConfig.appVersion = bcmMetricsConfig.appVersion;
        metricsConfig.reportQueueSize = bcmMetricsConfig.reportQueueSize;
        metricsConfig.metricsDir = bcmMetricsConfig.metricsDir;
        metricsConfig.metricsFileSizeInBytes = bcmMetricsConfig.metricsFileSizeInBytes;
        metricsConfig.metricsFileCount = bcmMetricsConfig.metricsFileCount;
        metricsConfig.reportIntervalInMs = bcmMetricsConfig.reportIntervalInMs;
        metricsConfig.clientId = bcmMetricsConfig.clientId;
        metricsConfig.writeThresholdInBytes = bcmMetricsConfig.writeThresholdInBytes;
    }
};

inline void to_json(nlohmann::json& j, const BCMMetricsConfig& config)
{
    j = nlohmann::json{{"appVersion", config.appVersion},
                       {"reportQueueSize", config.reportQueueSize},
                       {"metricsDir", config.metricsDir},
                       {"metricsFileSizeInBytes", config.metricsFileSizeInBytes},
                       {"metricsFileCount", config.metricsFileCount},
                       {"reportIntervalInMs", config.reportIntervalInMs},
                       {"clientId", config.clientId},
                       {"writeThresholdInBytes", config.writeThresholdInBytes}};
}

inline void from_json(const nlohmann::json& j, BCMMetricsConfig& config)
{
    jsonable::toString(j, "appVersion", config.appVersion);
    jsonable::toNumber(j, "reportQueueSize", config.reportQueueSize);
    jsonable::toString(j, "metricsDir", config.metricsDir);
    jsonable::toNumber(j, "metricsFileSizeInBytes", config.metricsFileSizeInBytes);
    jsonable::toNumber(j, "metricsFileCount", config.metricsFileCount);
    jsonable::toNumber(j, "reportIntervalInMs", config.reportIntervalInMs);
    jsonable::toString(j, "clientId", config.clientId);
    jsonable::toNumber(j, "writeThresholdInBytes", config.writeThresholdInBytes);
}

}