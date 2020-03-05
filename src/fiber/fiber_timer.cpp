#include "fiber_timer.h"

namespace bcm {

FiberTimer::FiberTimer()
    : m_execPool(1)
{
    m_execPool.run("fiber.timer");
}

FiberTimer::~FiberTimer()
{
    clear();
    m_execPool.stop();
}

void FiberTimer::schedule(const std::shared_ptr<Task>& task, int64_t intervalInMilli, bool bImmediatelyRun)
{
    FiberPool::post(m_execPool.getIOContext(), [=]() {
        m_tasks.emplace_back(task);

        if (bImmediatelyRun) {
            task->run();
        }

        while (!task->m_canceled) {
            auto waitTime = intervalInMilli - task->lastExecTimeInMilli();
            if (waitTime <= 0) {
                // exec too long, go to next round
                waitTime = intervalInMilli;
            }

            std::unique_lock<fibers::mutex> l(task->m_mtx);
            std::chrono::milliseconds duration(waitTime);
            task->m_cond.wait_for(l, duration);

            if (task->m_canceled) {
                break;
            }
            task->run();
        }

        m_tasks.remove(task);
    });
}

void FiberTimer::scheduleOnce(const std::shared_ptr<Task>& task, int64_t delayInMilli)
{
    FiberPool::post(m_execPool.getIOContext(), [=]() {
        if (delayInMilli <= 0) {
            task->run();
            return;
        }

        m_tasks.emplace_back(task);

        std::unique_lock<fibers::mutex> l(task->m_mtx);
        std::chrono::milliseconds duration(delayInMilli);
        task->m_cond.wait_for(l, duration);

        if (!task->m_canceled) {
            task->run();
        }
        m_tasks.remove(task);
    });
}

void FiberTimer::cancel(const std::shared_ptr<Task>& task)
{
    FiberPool::post(m_execPool.getIOContext(), [=] {
        {
            std::unique_lock<fibers::mutex> l(task->m_mtx);
            task->m_canceled = true;
        }
        task->m_cond.notify_all();
    });
}

void FiberTimer::clear()
{
    FiberPool::post(m_execPool.getIOContext(), [=] {
        for (auto& task : m_tasks) {
            this->cancel(task);
        }
    });
}

}