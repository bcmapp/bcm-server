#include "io_ctx_pool.h"
#include "utils/log.h"
#include "utils/thread_utils.h"

#include <functional>

namespace bcm {

IoCtxPool::IoCtxPool(int size) 
    : m_stopped(false)
{
    for (int i = 0; i < size; i++) {
        auto ioc = std::make_shared<boost::asio::io_context>();
        auto work = std::make_shared<boost::asio::io_context::work>(*ioc);
        m_iocs.emplace_back(ioc);
        m_works.emplace_back(work);
        m_threads.emplace_back([ioc, i]() {
            setCurrentThreadName("ioctxpool." + std::to_string(i));
            try {
                ioc->run();
            } catch (std::exception& e) {
                LOGE << "exception caught: " << e.what();
            }
        });
    }
}

IoCtxPool::~IoCtxPool()
{
    shutdown(true);
}

IoCtxPool::io_context_ptr IoCtxPool::getIoCtxByGid(uint64_t gid)
{
    if (!m_iocs.empty()) {
        return m_iocs[gid % m_iocs.size()];
    }
    return nullptr;
}

IoCtxPool::io_context_ptr IoCtxPool::getIoCtxByUid(const std::string& uid)
{
    if (!m_iocs.empty()) {
        std::size_t hash = std::hash<std::string>()(uid);
        return m_iocs[hash % m_iocs.size()];
    }
    return nullptr;
}

void IoCtxPool::shutdown(bool force)
{
    if (m_stopped) {
        return;
    }
    m_stopped = true;
    m_works.clear();
    if (force) {
        for (auto& ioc : m_iocs) {
            ioc->stop();
        }
    }
    for (auto& t : m_threads) {
        t.join();
    }
}

} // namespace bcm