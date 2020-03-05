#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>

#include <boost/asio.hpp>
#include <boost/assert.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/config.hpp>

#include <boost/fiber/condition_variable.hpp>
#include <boost/fiber/context.hpp>
#include <boost/fiber/mutex.hpp>
#include <boost/fiber/operations.hpp>
#include <boost/fiber/scheduler.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {
namespace asio {

class round_robin : public algo::algorithm {
public:
    explicit round_robin(std::shared_ptr<boost::asio::io_context>  ioc);

    void awakened(context* ctx) noexcept override;
    context* pick_next() noexcept override;
    bool has_ready_fibers() const noexcept override;
    void suspend_until(std::chrono::steady_clock::time_point const& abs_time) noexcept override;
    void notify() noexcept override;

    void run() noexcept;

    class service : public boost::asio::io_context::service {
    public:
        static boost::asio::io_context::id id;

        explicit service(boost::asio::io_context& ioc)
            : boost::asio::io_context::service(ioc)
            , m_work{new boost::asio::io_context::work(ioc)}
        {
        }

        ~service() override = default;

        service(service const&) = delete;

        service& operator=(service const&) = delete;

        void shutdown_service() final
        {
            m_work.reset();
        }

    private:
        std::unique_ptr<boost::asio::io_context::work> m_work;
    };

private:
    std::shared_ptr<boost::asio::io_context> m_ioc;
    boost::asio::steady_timer m_suspendTimer;

    boost::fibers::scheduler::ready_queue_type m_readyQueue{};
    boost::fibers::mutex m_mutex{};
    boost::fibers::condition_variable m_cnd{};
    std::size_t m_counter{0};
};

}
}
}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif






