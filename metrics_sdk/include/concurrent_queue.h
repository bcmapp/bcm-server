#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

namespace bcm {
namespace metrics {

template<typename T>
class ConcurrentQueue
{
public:
    ConcurrentQueue()
    {
        m_size = 0;
    }

    ConcurrentQueue(uint32_t size)
        : m_size(size)
    {
    }

    ~ConcurrentQueue() = default;

public:
    // try enqueue a entry, when:
    // queue has size: drop entry when queue is full, and return false
    // queue hasn't size: always put into queue and return true
    bool tryEnqueue(T&& entry)
    {
        std::unique_lock<std::mutex> lk(m_queueMutex);
        if (m_size != 0 && m_queue.size() >= m_size) {
            return false;
        }
        bool wakeup = m_queue.empty();
        m_queue.push(std::move(entry));
        if (wakeup) {
            m_queueCond.notify_one();
        }
        return true;
    }

    bool tryEnqueue(const T& entry)
    {
        std::unique_lock<std::mutex> lk(m_queueMutex);
        if (m_size != 0 && m_queue.size() >= m_size) {
            return false;
        }
        bool wakeup = m_queue.empty();
        m_queue.push(entry);
        if (wakeup) {
            m_queueCond.notify_one();
        }
        return true;
    }

    // must put into the queue
    void enqueue(const T& entry)
    {
        std::unique_lock<std::mutex> lk(m_queueMutex);
        bool wakeup = m_queue.empty();
        m_queue.push(entry);
        if (wakeup) {
            m_queueCond.notify_one();
        }
    }

    void enqueue(T&& entry)
    {
        std::unique_lock<std::mutex> lk(m_queueMutex);
        bool wakeup = m_queue.empty();
        m_queue.push(std::move(entry));
        if (wakeup) {
            m_queueCond.notify_one();
        }
    }

    // pop a entry from queue, wait if empty
    void blockingPop(T& entry)
    {
        std::unique_lock<std::mutex> lk(m_queueMutex);
        m_queueCond.wait(lk, [&] { return !m_queue.empty(); });
        entry = m_queue.front();
        m_queue.pop();
    }

    // pop a entry from queue, return false when empty
    bool tryPop(T& entry)
    {
        std::unique_lock<std::mutex> lk(m_queueMutex);
        if (m_queue.empty()) {
            return false;
        }
        entry = m_queue.front();
        m_queue.pop();
        return true;
    }


private:
    std::mutex m_queueMutex;
    std::condition_variable m_queueCond;
    std::queue<T> m_queue;
    uint32_t m_size;

};


} //namespace
}
