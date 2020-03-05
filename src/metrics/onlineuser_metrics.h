#pragma once

#include <fiber/fiber_timer.h>
#include <memory>
#include "dispatcher/dispatch_manager.h"
#include "utils/thread_utils.h"
#include <thread>

namespace bcm
{

class OnlineUserMetrics
{
public:
    OnlineUserMetrics(std::shared_ptr<DispatchManager> dispatchManager,
                       int64_t reportIntervalInMs);

    ~OnlineUserMetrics() = default;

    void start();

private:
    std::shared_ptr<DispatchManager> m_dispatchManager;
    int64_t m_reportIntervalInMs;
    std::thread m_reportThread;

};

}