//
// Created by catror on 18-10-11.
//

#include "asio_round_robin.h"

namespace boost {
namespace fibers {
namespace asio {

boost::asio::io_context::id round_robin::service::id;

round_robin::round_robin(std::shared_ptr<boost::asio::io_context>  ioc)
    : m_ioc(std::move(ioc))
    , m_suspendTimer(*m_ioc)
{
    boost::asio::add_service(*m_ioc, new service(*m_ioc));
    fibers::context::active()->get_scheduler()->set_algo(this);
}

void round_robin::awakened(boost::fibers::context* ctx) noexcept
{
    BOOST_ASSERT(nullptr != ctx);
    BOOST_ASSERT(!ctx->ready_is_linked());
    ctx->ready_link(m_readyQueue); /*< fiber, enqueue on ready queue >*/
    if (!ctx->is_context(boost::fibers::type::dispatcher_context)) {
        ++m_counter;
    }
}

context* round_robin::pick_next() noexcept
{
    context* ctx(nullptr);
    if (!m_readyQueue.empty()) { /*<
            pop an item from the ready queue
        >*/
        ctx = &m_readyQueue.front();
        m_readyQueue.pop_front();
        BOOST_ASSERT(nullptr != ctx);
        BOOST_ASSERT(context::active() != ctx);
        if (!ctx->is_context(boost::fibers::type::dispatcher_context)) {
            --m_counter;
        }
    }
    return ctx;
}

bool round_robin::has_ready_fibers() const noexcept
{
    return 0 < m_counter;
}

void round_robin::suspend_until(std::chrono::steady_clock::time_point const& abs_time) noexcept
{
    // Set a timer so at least one handler will eventually fire, causing
    // run_one() to eventually return.
    if ((std::chrono::steady_clock::time_point::max) () != abs_time) {
        // Each expires_at(time_point) call cancels any previous pending
        // call. We could inadvertently spin like this:
        // dispatcher calls suspend_until() with earliest wake time
        // suspend_until() sets suspend_timer_
        // lambda loop calls run_one()
        // some other asio handler runs before timer expires
        // run_one() returns to lambda loop
        // lambda loop yields to dispatcher
        // dispatcher finds no ready fibers
        // dispatcher calls suspend_until() with SAME wake time
        // suspend_until() sets suspend_timer_ to same time, canceling
        // previous async_wait()
        // lambda loop calls run_one()
        // asio calls suspend_timer_ handler with operation_aborted
        // run_one() returns to lambda loop... etc. etc.
        // So only actually set the timer when we're passed a DIFFERENT
        // abs_time value.
        m_suspendTimer.expires_at(abs_time);
        m_suspendTimer.async_wait([](boost::system::error_code const&) {
            this_fiber::yield();
        });
    }
    m_cnd.notify_one();
}

void round_robin::notify() noexcept
{
    // Something has happened that should wake one or more fibers BEFORE
    // suspend_timer_ expires. Reset the timer to cause it to fire
    // immediately, causing the run_one() call to return. In theory we
    // could use cancel() because we don't care whether suspend_timer_'s
    // handler is called with operation_aborted or success. However --
    // cancel() doesn't change the expiration time, and we use
    // suspend_timer_'s expiration time to decide whether it's already
    // set. If suspend_until() set some specific wake time, then notify()
    // canceled it, then suspend_until() was called again with the same
    // wake time, it would match suspend_timer_'s expiration time and we'd
    // refrain from setting the timer. So instead of simply calling
    // cancel(), reset the timer, which cancels the pending sleep AND sets
    // a new expiration time. This will cause us to spin the loop twice --
    // once for the operation_aborted handler, once for timer expiration
    // -- but that shouldn't be a big problem.
    m_suspendTimer.async_wait([](boost::system::error_code const&) {
        this_fiber::yield();
    });
    m_suspendTimer.expires_at(std::chrono::steady_clock::now());
}

void round_robin::run() noexcept
{
    while (!m_ioc->stopped()) {
        if (has_ready_fibers()) {
            // run all pending handlers in round_robin
            while (m_ioc->poll());
            // block this fiber till all pending (ready) fibers are processed
            // == round_robin::suspend_until() has been called
            std::unique_lock<boost::fibers::mutex> lk(m_mutex);
            m_cnd.wait(lk);
        } else {
            // run one handler inside io_context
            // if no handler available, block this thread
            if (!m_ioc->run_one()) {
                break;
            }
        }
    }
}

}
}
}

