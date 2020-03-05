#pragma once

namespace bcm {
namespace metrics {

struct MetricsConfig {
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
    // metrics client id, must be 5 character
    std::string clientId;
    // metrics output Threshold
    int64_t writeThresholdInBytes;
};

}
}