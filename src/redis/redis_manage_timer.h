#pragma once


#include "utils/time.h"
#include "fiber/fiber_timer.h"
#include "redis/redis_manager.h"

namespace bcm
{

class RedisManageTimer : public FiberTimer::Task {
public:
    RedisManageTimer(RedisDbManager* redisDbManager);

    static constexpr int redisManageTimerInterval = 5000; // ms

private:
    void run() override;
    void cancel() override;
    int64_t lastExecTimeInMilli() override;

    void loop(void);
private:
    RedisDbManager* m_redisDbManager;
    int64_t m_execTime;
};



} // namespace bcm
