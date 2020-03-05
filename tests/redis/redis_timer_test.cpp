#include "../test_common.h"
#include "config/group_store_format.h"
#include "config/bcm_config.h"
#include "redis/redis_manager.h"
#include "redis/redis_manage_timer.h"
#include "fiber/fiber_timer.h"

using namespace bcm;

void readConfig(BcmConfig& config, std::string configFile){
    std::cout << "read config file: " << configFile << std::endl;

    std::ifstream fin(configFile, std::ios::in);
    std::string content;
    char buf[4096] = {0};
    while (fin.good()) {
        fin.read(buf, 4096);
        content.append(buf, static_cast<unsigned long>(fin.gcount()));
    }

    nlohmann::json j = nlohmann::json::parse(content);
    config = j.get<BcmConfig>();
}

TEST_CASE("redis_timer")
{
    BcmConfig config;
    readConfig(config, "./config.json");

    REQUIRE(true == RedisDbManager::Instance()->setRedisDbConfig(config.groupRedis));
    
    auto redisManageTimer = std::make_shared<RedisManageTimer>(RedisDbManager::Instance());

    auto fiberTimer = std::make_shared<FiberTimer>();
    fiberTimer->schedule(redisManageTimer, RedisManageTimer::redisManageTimerInterval, true);


    sleep(30);

    auto ptrRedisServer00 = std::make_shared<RedisServer>(config.groupRedis["p0"]["0"].ip,
                                                          config.groupRedis["p0"]["0"].port,
                                                          config.groupRedis["p0"]["0"].password,
                                                          config.groupRedis["p0"]["0"].regkey);
    
    std::string val = "";
    REQUIRE( true == ptrRedisServer00->getRedisConn()->get(REDISDB_KEY_GROUP_REDIS_ACTIVE, val));
    REQUIRE("active" == val);


    auto ptrRedisServer01 = std::make_shared<RedisServer>(config.groupRedis["p1"]["0"].ip,
                                                          config.groupRedis["p1"]["0"].port,
                                                          config.groupRedis["p1"]["0"].password,
                                                          config.groupRedis["p1"]["0"].regkey);

    val = "";
    REQUIRE( true == ptrRedisServer01->getRedisConn()->get(REDISDB_KEY_GROUP_REDIS_ACTIVE, val));
    REQUIRE("active" == val);

    fiberTimer->cancel(redisManageTimer);
    sleep(3);
    return;
}
