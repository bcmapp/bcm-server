#pragma once

#include <mutex>
#include <condition_variable>
#include "log.h"

namespace bcm {

class SyncLatch {
public:
    explicit SyncLatch(size_t count)
        : m_count(count)
        , m_generation(0)
    {
    }

    ~SyncLatch() = default;

    void sync()
    {
        std::unique_lock<std::mutex> lk(m_mutex);
        size_t generation(m_generation);
        if (--m_count == 0) {
            ++m_generation;
            lk.unlock();
            m_cond.notify_all();
            return;
        }
        while (generation == m_generation) {
            m_cond.wait(lk);
        }
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cond;
    size_t m_count;
    size_t m_generation;
};

}
