#include "fiber_pool.h"
#include "asio_round_robin.h"
#include <utils/log.h>
#include <utils/sync_latch.h>
#include <utils/thread_utils.h>

namespace bcm {

using namespace boost;

static thread_local asio::io_context* s_tlsIoc = nullptr;

FiberPool::FiberPool(size_t concurrency)
{
    for (size_t i = 0; i < concurrency; ++i) {
        m_iocs.push_back(std::make_shared<asio::io_context>());
    }
}

void FiberPool::run(const std::string& name)
{
    auto sl = std::make_shared<SyncLatch>(m_iocs.size() + 1);
    for (auto& ioc : m_iocs) {
        m_threads.emplace_back([sl, &ioc, name]() {
            setCurrentThreadName(name);
            fibers::asio::round_robin rr(ioc);
            s_tlsIoc = ioc.get();
            sl->sync();
            rr.run();
            s_tlsIoc = nullptr;
        });
    }
    sl->sync();
    LOGD << "fiber pool is running for " << name;
}

void FiberPool::stop()
{
    for (auto& ioc : m_iocs) {
        ioc->stop();
    }

    for (auto& t : m_threads) {
        t.join();
    }
}

asio::io_context& FiberPool::getIOContext()
{
    uint64_t index = static_cast<uint64_t>(m_nextIocIndex++);
    return *m_iocs[index % static_cast<uint64_t>(m_iocs.size())];
}

asio::io_context* FiberPool::getThreadIOContext()
{
    return s_tlsIoc;
}

}

