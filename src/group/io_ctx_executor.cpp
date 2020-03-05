#include "io_ctx_executor.h"
#include "utils/log.h"
#include "utils/thread_utils.h"

namespace bcm {

IoCtxExecutor::IoCtxExecutor(int size)
    : m_ioc(std::make_shared<boost::asio::io_context>())
    , m_work(std::make_shared<boost::asio::io_context::work>(*m_ioc))
{
    for (int i = 0; i < size; i++) {
        m_threads.emplace_back([this, i]() mutable {
            setCurrentThreadName("ioctxexec." + std::to_string(i));
            try {
                m_ioc->run();
            } catch (std::exception& e) {
                LOGE << "exception caught: " << e.what();
            }
        });
    }
}

IoCtxExecutor::~IoCtxExecutor()
{
    stop(true);
}

void IoCtxExecutor::stop(bool force)
{
    if (!m_ioc->stopped()) {
        m_work.reset();
        if (force) {
            m_ioc->stop();
        }
        for (auto& t : m_threads) {
            t.join();
        }   
    }
}

} // namespace bcm