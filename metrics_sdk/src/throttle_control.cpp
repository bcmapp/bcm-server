#include "throttle_control.h"
#include "metrics_utils.h"
#include "metrics_log_utils.h"
#include <thread>
#include <inttypes.h>
#include <future>

namespace bcm {
namespace metrics {

ThrottleControl::ThrottleControl(int64_t readThreshold, int64_t writeThreshold)
    : m_readThreshold(readThreshold)
    , m_writeThreshold(writeThreshold)
{
}

ThrottleControl::~ThrottleControl(void) {}

// https://en.cppreference.com/w/cpp/thread/condition_variable
//
// The thread that intends to modify the variable has to
//    - acquire a std::mutex (typically via std::lock_guard)
//    - perform the modification while the lock is held
//    - execute notify_one or notify_all on the std::condition_variable
//      (the lock does not need to be held for notification)
//      Even if the shared variable is atomic, it must be modified under the mutex
//      in order to correctly publish the modification to the waiting thread.
//
// Any thread that intends to wait on std::condition_variable has to
//    - acquire a std::unique_lock<std::mutex>, on the same mutex as used to
//      protect the shared variable
//    - execute wait, wait_for, or wait_until. The wait operations atomically
//      release the mutex and suspend the execution of the thread.
//    - When the condition variable is notified, a timeout expires,
//      or a spurious wakeup occurs, the thread is awakened, and the mutex is atomically reacquired.
//      The thread should then check the condition and resume waiting if the wake up was spurious.
void ThrottleControl::start(void)
{
    std::promise<void> promise;
    std::future<void> future = promise.get_future();
    // Spawn individual thread to renew threshold counter
    std::thread thd([&]() {
        std::string threadName = "metrics.throttle";
        setThreadName(threadName);
        METRICS_LOG_INFO("%s started!", threadName.c_str());

        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_readQuota = m_readThreshold;
            m_writeQuota = m_writeThreshold;
        }

        promise.set_value();
        m_cv.notify_all();

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(60));

            {
                std::unique_lock<std::mutex> lk(m_mutex);
                m_readQuota = m_readThreshold;
                m_writeQuota = m_writeThreshold;
            }

            m_cv.notify_all();
        } // while
    });

    thd.detach();
    future.wait();
}

// Check if read quota is available, block calling thread if quota is exhausted
void ThrottleControl::checkReadQuota(uint32_t n)
{
    std::unique_lock<std::mutex> lk(m_mutex);
    do {
        m_readQuota = m_readQuota - n;
        if (m_readQuota >= 0) {
            break;
        } else {
            METRICS_LOG_INFO("Read over quota: %u/%" PRId64, n, m_readQuota);
            m_cv.wait(lk);
        }
    } while (true);
}

// Check if write quota is available, block calling thread if quota is exhausted
void ThrottleControl::checkWriteQuota(uint32_t n)
{
    std::unique_lock<std::mutex> lk(m_mutex);
    do {
        m_writeQuota = m_writeQuota - n;
        if (m_writeQuota >= 0) {
            break;
        } else {
            METRICS_LOG_INFO("Write over quota: %u/%" PRId64, n, m_writeQuota);
            m_cv.wait(lk);
        }
    } while (true);
}

} // namespace
}
