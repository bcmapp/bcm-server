#include "libevent_utils.h"

#include <event2/event.h>
#include <event2/util.h>
#include <boost/core/ignore_unused.hpp>

namespace bcm {
namespace libevent {
// -----------------------------------------------------------------------------
// Section: AsyncTask
// -----------------------------------------------------------------------------
AsyncTask::AsyncTask(struct event_base* eb)
{
    event_assign(&m_evt, eb, -1, 0, onRun, reinterpret_cast<void*>(this) );
}

AsyncTask::~AsyncTask()
{
    event_del(&m_evt);
}

void AsyncTask::execute()
{
    event_add(&m_evt, NULL);
    event_active(&m_evt, 0, 0);
}

void AsyncTask::executeWithDelay(int millisecs)
{
    timeval tv;
    evutil_timerclear(&tv);
    tv.tv_sec = (int)(millisecs / (int)1000);
    tv.tv_usec = (millisecs % 1000) * 1000;
    event_add(&m_evt, &tv);
}

// static
void AsyncTask::onRun(int sock, short which, void* arg)
{
    boost::ignore_unused(sock, which);
    AsyncTask* task = reinterpret_cast<AsyncTask*>(arg);
    event_del(&task->m_evt);
    task->run();
}

// -----------------------------------------------------------------------------
// Section: AsyncFunc
// -----------------------------------------------------------------------------
AsyncFunc::AsyncFunc(struct event_base* eb, Fn&& fn)
    : AsyncTask(eb), m_fn(std::move(fn)) 
{
}

void AsyncFunc::run()
{
    m_fn();
    delete this;
}

// static 
void AsyncFunc::invoke(struct event_base* eb, Fn&& fn)
{
    (new AsyncFunc(eb, std::forward<Fn>(fn)))->execute();
}

} // namespace libevent
} // namespace bcm