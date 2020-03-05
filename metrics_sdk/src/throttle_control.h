#pragma once

#include <mutex>
#include <condition_variable>

namespace bcm {
namespace metrics {

class ThrottleControl {
public:
    explicit ThrottleControl(int64_t readThreshold, int64_t writeThreshold);
    ~ThrottleControl(void);

public:
    void start(void);
    // Check if quota is available, block calling thread if quota is exhausted
    void checkReadQuota(uint32_t n);
    void checkWriteQuota(uint32_t n);

private:
    int64_t m_readThreshold = 0;
    int64_t m_writeThreshold = 0;
    int64_t m_readQuota = 0;
    int64_t m_writeQuota = 0;
    std::condition_variable m_cv;
    std::mutex m_mutex;
};

} // namespace
}
