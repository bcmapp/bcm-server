#pragma once

#include "fiber_pool.h"
#include <list>


namespace bcm {

class FiberTimer {
public:
    class Task {
    public:
        virtual void run() = 0;
        virtual void cancel() {};
        virtual int64_t lastExecTimeInMilli() { return 0; };

    private:
        fibers::condition_variable m_cond;
        fibers::mutex m_mtx;
        bool m_canceled{false};

        friend class FiberTimer;
    };

    FiberTimer();
    ~FiberTimer();

    void schedule(const std::shared_ptr<Task>& task, int64_t intervalInMilli, bool bImmediatelyRun = false);
    void scheduleOnce(const std::shared_ptr<Task>& task, int64_t delayInMilli);
    void cancel(const std::shared_ptr<Task>& task);

    void clear();

private:
    FiberPool m_execPool;
    std::list<std::shared_ptr<Task>> m_tasks;
};

}