#include "../test_common.h"
#include "config/redis_config.h"
#include <event2/thread.h>
#include "redis/async_conn.h"
#include "redis/reply.h"
#include <hiredis/hiredis.h>
#include "redis/online_redis_manager.h"

using namespace bcm;

class SubPubHandler : public redis::AsyncConn::ISubscriptionHandler {
public:
    void onSubscribe(const std::string& chan) {
        TLOG << "### user handle subscribe channel: " << chan;
    }
    void onUnsubscribe(const std::string& chan) {
        TLOG << "### user handle unsubscribe channel: " << chan;
    }
    void onMessage(const std::string& chan, const std::string& msg) {
        TLOG << "#################### user handler sub receive msg: " << msg << ", channel: " << chan;
    }
    void onError(int code) {
        TLOG << "### user sub receive error code: " << code;
    }
};

TEST_CASE("online_redis_manager")
{
    evthread_use_pthreads();

    std::map<std::string, std::vector<RedisConfig>> pRedis;
    std::vector<RedisConfig> vec1, vec2;
    vec1.emplace_back(RedisConfig{"127.0.0.1", 6376, "", ""});
    vec1.emplace_back(RedisConfig{"127.0.0.1", 6377, "", ""});
    vec2.emplace_back(RedisConfig{"127.0.0.1", 6378, "", ""});
    vec2.emplace_back(RedisConfig{"127.0.0.1", 6379, "", ""});

    pRedis["p0"] = vec1;
    pRedis["p1"] = vec2;

    SubPubHandler h;

    OnlineRedisManager::Instance()->init(pRedis);
    OnlineRedisManager::Instance()->start();
    
    sleep(2);

    //使用场景一：
    OnlineRedisManager::Instance()->subscribe("group_1", &h);
    OnlineRedisManager::Instance()->psubscribe("group_psub_*", &h);
    sleep(2);
    OnlineRedisManager::Instance()->publish("group_1", "group_1", "hello",
                                        [](int status, const redis::Reply& reply) {
        REQUIRE(REDIS_OK == status);
        REQUIRE(reply.getInteger() == 1);
        TLOG << "publish group_1 hello success !"; 
    });

    OnlineRedisManager::Instance()->publish("group_psub_*", "group_psub_1", "hello_psub",
                                        [](int status, const redis::Reply& reply) {
        REQUIRE(REDIS_OK == status);
        REQUIRE(reply.getInteger() == 1);
        TLOG << "publish group_psub_1 success !";
    });

    OnlineRedisManager::Instance()->publish("group_1", "hello_banana",
                                            [](int status, const redis::Reply& reply) {
        REQUIRE(REDIS_OK == status);
        REQUIRE(reply.getInteger() == 1);

        TLOG << "publish group_1 hello_banana success !";
    });

    //场景二：
    OnlineRedisManager::Instance()->unsubscribe("group_1");
    OnlineRedisManager::Instance()->punsubscribe("group_psub_*");
    sleep(2);
    OnlineRedisManager::Instance()->publish("group_1", "group_1", "hello",
                                    [](int status, const redis::Reply& reply) {
        REQUIRE(REDIS_OK == status);
        REQUIRE(reply.getInteger() == 0);
    });

    OnlineRedisManager::Instance()->publish("group_psub_*", "group_psub_1", "hello_psub",
                                        [](int status, const redis::Reply& reply) {
        REQUIRE(REDIS_OK == status);
        REQUIRE(reply.getInteger() == 0);
        TLOG << "publish group_psub_1 success !";
    });

    //使用场景三：
    //publish 异步-同步
    OnlineRedisManager::Instance()->subscribe("group_0", &h);
    sleep(2);
    OnlineRedisManager::Instance()->publish("group_0", "handler is nullptr");
    std::promise<int32_t> promise;
    std::future<int32_t> future = promise.get_future();
    OnlineRedisManager::Instance()->publish("group_0", "sync_publish", 
                                            [&promise](int status, const redis::Reply& reply) {
        REQUIRE(REDIS_OK == status);
        REQUIRE(reply.getInteger() == 1);
        promise.set_value(reply.getInteger());
    });
    REQUIRE(future.get() == 1);

    sleep(3);
}
