#pragma once

#include <functional>
#include <event2/event_struct.h>

namespace bcm {
namespace libevent {
// -----------------------------------------------------------------------------
// Section: AsyncTask
// -----------------------------------------------------------------------------
class AsyncTask {
public:
    explicit AsyncTask(struct event_base* eb);
    virtual ~AsyncTask();

    void execute();
    void executeWithDelay(int millisecs);

protected:
    static void onRun(int sock, short which, void* arg);
    virtual void run() = 0;

protected:
    struct event m_evt;
};

// -----------------------------------------------------------------------------
// Section: AsyncFunc
// -----------------------------------------------------------------------------
class AsyncFunc : public AsyncTask {
    typedef std::function<void()> Fn;

public:
    AsyncFunc(struct event_base* eb, Fn&& fn);

    static void invoke(struct event_base* eb, Fn&& fn);

protected:
    void run() override;

private:
    Fn m_fn;
};

} // namespace libevent
} // namespace bcm