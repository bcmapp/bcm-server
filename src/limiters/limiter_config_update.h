#pragma once
#include "fiber/fiber_timer.h"

namespace bcm {
    
class LimiterConfigUpdate : public FiberTimer::Task {
public:
    virtual void run();
};

}
