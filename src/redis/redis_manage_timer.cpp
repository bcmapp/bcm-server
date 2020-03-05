#include "redis_manage_timer.h"
namespace bcm {

RedisManageTimer::RedisManageTimer(RedisDbManager* redisDbManager)
:m_redisDbManager(redisDbManager)
{
}

void RedisManageTimer::run()
{
    int64_t start = nowInMilli();
    
    m_redisDbManager->updateRedisConnPeriod();

    m_execTime = nowInMilli() - start;
}

void RedisManageTimer::cancel()
{
}

int64_t RedisManageTimer::lastExecTimeInMilli()
{
    return m_execTime;
}

} // namespace bcm