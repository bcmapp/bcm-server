#include "../test_common.h"
#include "fiber/fiber_timer.h"
#include "redis/redis_manager.h"
#include "config/redis_config.h"
#include "config/group_store_format.h"
#include "dao/client.h"
#include "proto/dao/group_msg.pb.h"
#include "group/message_type.h"
#include "utils/time.h"
#include "redis/redis_manage_timer.h"




using namespace bcm;

void init(std::unordered_map<std::string, std::unordered_map<std::string, RedisConfig> > &redisDb) {

    // RedisConfig c00 = {"183.60.218.152", 6380, "", ""}, c01 = {"183.60.218.152", 6381, "", ""}, 
    //             c10 = {"183.60.218.152", 6382, "", ""}, c11 = {"183.60.218.152", 6383, "", ""};
    RedisConfig c00 = {"127.0.0.1", 6376, "", ""}, c01 = {"127.0.0.1", 6377, "", ""}, 
                c10 = {"127.0.0.1", 6378, "", ""}, c11 = {"127.0.0.1", 6379, "", ""};
    std::unordered_map<std::string, RedisConfig> m0, m1;
    m0["0"] = c00;
    m0["1"] = c01;
    m1["0"] = c10;
    m1["1"] = c11;
    redisDb["p0"] = m0;
    redisDb["p1"] = m1;
}

TEST_CASE("redis_mgr") {
    std::unordered_map<std::string, std::unordered_map<std::string, RedisConfig> > redisDb;
    init(redisDb);

    std::cout << "www test ..." << std::endl;

    REQUIRE(6376 == redisDb["p0"]["0"].port);
    REQUIRE(6377 == redisDb["p0"]["1"].port);
    REQUIRE(6378 == redisDb["p1"]["0"].port);
    REQUIRE(6379 == redisDb["p1"]["1"].port);

    RedisDbManager::Instance()->setRedisDbConfig(redisDb);

    // hset
    std::string setValue = "{'from_uid':'000000000002','last_mid': 55}";
    REQUIRE(true == RedisDbManager::Instance()->hset(10000, "group_test", "uid:0000000001", setValue));  // 6382
    // hget
    std::string getValue = "";
    REQUIRE(true == RedisDbManager::Instance()->hget(10000, "group_test", "uid:0000000001", getValue));
    REQUIRE(getValue == setValue);
    getValue = "";
    REQUIRE(true == RedisDbManager::Instance()->hget(10000, "group_test_not_exist", "uid:0000000002", getValue));
    REQUIRE(getValue == "");

    // hmset
    std::vector<HField> values;
    values.emplace_back("uid:000000000003","{'last_mid': 60}");  // 加空格
    values.emplace_back("uid:000000000004","{'last_mid': 65}");
    values.emplace_back("uid:000000000005","{'last_mid': 66}");
    REQUIRE(true == RedisDbManager::Instance()->hmset(10001, "group_test", values));
    // hget
    getValue = "";
    REQUIRE(true == RedisDbManager::Instance()->hget(10001, "group_test", "uid:000000000003", getValue));
    REQUIRE(getValue == "{'last_mid': 60}");
    // hmget    hmget(uint64_t gid, const std::string& key, const std::vector<std::string>& fields, std::map<std::string, std::string>& mapFieldValue)
    std::vector<std::string> hmgetField{"uid:0000000001","uid:000000000003","uid:000000000004","uid:000000000005"};
    std::map<std::string, std::string> hmgetMap;
    REQUIRE(true == RedisDbManager::Instance()->hmget(10001, "group_test", hmgetField, hmgetMap));
    REQUIRE("{'from_uid':'000000000002','last_mid': 55}" == hmgetMap["uid:0000000001"]);
    REQUIRE("{'last_mid': 60}" == hmgetMap["uid:000000000003"]);
    REQUIRE("{'last_mid': 65}" == hmgetMap["uid:000000000004"]);
    REQUIRE("{'last_mid': 66}" == hmgetMap["uid:000000000005"]);

    // hdel
    REQUIRE(true == RedisDbManager::Instance()->hdel(10001, "group_test", hmgetField));
    getValue = "";
    REQUIRE(true == RedisDbManager::Instance()->hget(10001, "group_test", "uid:000000000003", getValue));
    REQUIRE(getValue == "");

    // zadd
    REQUIRE(true == RedisDbManager::Instance()->zadd(10002, "group_test", "zadd_member_1", 100));
    
    // set
    std::string  thrKey = "uid:000000000009";
    REQUIRE(true == RedisDbManager::Instance()->set(thrKey, thrKey, "a"));

    uint64_t new_value = 0;
    int32_t ret = RedisDbManager::Instance()->incr(thrKey, thrKey, new_value);
    REQUIRE(0 == ret);

    REQUIRE(true == RedisDbManager::Instance()->set(thrKey, thrKey, "1"));
    REQUIRE(true == RedisDbManager::Instance()->expire(thrKey, thrKey, 10));

    ret = RedisDbManager::Instance()->incr(thrKey, thrKey, new_value);
    REQUIRE(2 == new_value);

    uint64_t ttl =RedisDbManager::Instance()->ttl(thrKey, thrKey);
    REQUIRE(ttl > 8 );
    
    RedisDbManager::Instance()->del(thrKey, thrKey);
    ret = RedisDbManager::Instance()->incr(thrKey, thrKey, new_value);
    REQUIRE(1 == new_value);
    
}
