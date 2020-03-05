#include "../test_common.h"
#include "redis/hiredis_client.h"
#include "redis/reply.h"

#include <event2/event.h>
#include <event2/thread.h>
#include <hiredis/hiredis.h>
#include <boost/core/ignore_unused.hpp>

#include <thread>

#include "utils/time.h"

using namespace bcm;

TEST_CASE("GeneralCmd")
{
    bcm::RedisConfig redisCfg;
    //redisCfg.ip = "127.0.0.1";
    //redisCfg.port = 6379;
    redisCfg.ip = "183.60.218.152";
    redisCfg.port = 6381;
    redisCfg.password = "";
    redisCfg.regkey = "";

    std::vector<bcm::RedisConfig> redisCfgVec;
    redisCfgVec.emplace_back(std::move(redisCfg));

    RedisClientSync* rcs = bcm::RedisClientSync::Instance();
    rcs->setRedisConfig(redisCfgVec);

    std::cout << std::endl << "--- cleanup ---" << std::endl;

    
    {
        std::vector<std::string>  vecDelete;
        std::string new_cursor = "";
        std::map<std::string, std::string> v;
        REQUIRE(rcs->hscan(0, "general_test_key123", "0", "", 0, new_cursor, v) == true);
        std::cout << "new cursor: " << new_cursor << std::endl;
        for (auto& k : v) {
            std::cout << "---delete: " << k.first << "," << k.second << std::endl;
            vecDelete.emplace_back(k.first);
        }
        REQUIRE(rcs->hdel(0, "general_test_key123", vecDelete));
    }
    
    
    // hset
    std::cout << std::endl << "--- hset ---" << std::endl;
    REQUIRE(rcs->hset("general_test_key123", "test_field1", "test_field1_value") == true);
    std::string r;
    REQUIRE(rcs->hget("general_test_key123", "test_field1", r) == true);
    REQUIRE(r == "test_field1_value");
    
    // hlen
    std::cout << std::endl << "--- hlen ---" << std::endl;
    uint64_t num;
    REQUIRE(rcs->hlen("general_test_key123", num) == true);
    REQUIRE(num == 1);
    REQUIRE(rcs->hset("general_test_key123", "test_field2", "test_field2_value") == true);
    REQUIRE(rcs->hlen("general_test_key123", num) == true);
    REQUIRE(num == 2);
    REQUIRE(rcs->hset("general_test_key123", "test_field3", "test_field3_value") == true);
    REQUIRE(rcs->hlen("general_test_key123", num) == true);
    REQUIRE(num == 3);

    // hdel
    std::cout << std::endl << "--- hdel ---" << std::endl;
    REQUIRE(rcs->hdel("general_test_key123", "test_field3") == true);
    r = "";
    REQUIRE(rcs->hget("general_test_key123", "test_field3", r) == true);
    REQUIRE(r == "");

    // hsetnx
    std::cout << std::endl << "--- hsetnx ---" << std::endl;
    REQUIRE(rcs->hsetnx("general_test_key123", "test_field1", "test_field1_value2") == true);
    REQUIRE(rcs->hget("general_test_key123", "test_field1", r) == true);
    REQUIRE(r == "test_field1_value");

    // hscan
    std::cout << std::endl << "--- hscan ---" << std::endl;
    std::string new_cursor = "";
    std::map<std::string, std::string> v;
    REQUIRE(rcs->hscan(0, "general_test_key123", "0", "", 0, new_cursor, v) == true);
    std::cout << "new cursor: " << new_cursor << std::endl;
    for (auto& k : v) {
        std::cout << "--- " << k.first << "," << k.second << std::endl;
    }

    // hmset
    std::cout << std::endl << "--- hmset ---" << std::endl;
    HField h1("test_field1", "test_field1_value101");
    HField h2("test_field2", "test_field2_value102");
    HField h3("test_field3", "test_field3_value103");
    std::vector<HField> vls;
    vls.emplace_back(std::move(h1));
    vls.emplace_back(std::move(h2));
    vls.emplace_back(std::move(h3));
    REQUIRE(rcs->hmset(0, "general_test_key123", vls) == true);

    // hmget
    std::cout << std::endl << "--- hmget ---" << std::endl;
    std::vector<std::string>   vecLs;
    vecLs.push_back("test_field1");
    vecLs.push_back("test_field4");
    vecLs.push_back("test_field2");
    vecLs.push_back("test_field3");

    std::map<std::string, std::string> mapFieldValue;
    REQUIRE(rcs->hmget(0, "general_test_key123", vecLs, mapFieldValue) == true);
    REQUIRE(mapFieldValue.size() == 4);
    REQUIRE(mapFieldValue["test_field1"] == "test_field1_value101");
    REQUIRE(mapFieldValue["test_field2"] == "test_field2_value102");
    REQUIRE(mapFieldValue["test_field3"] == "test_field3_value103");
    REQUIRE(mapFieldValue["test_field4"] == "");

    
    new_cursor = "";
    v.clear();
    REQUIRE(rcs->hscan(0, "general_test_key123", "0", "", 0, new_cursor, v) == true);
    std::cout << "new cursor: " << new_cursor << std::endl;
    for (auto& k : v) {
        std::cout << "--- " << k.first << "," << k.second << std::endl;
    }
    
    // zadd
    int64_t tm = nowInSec();
    REQUIRE(rcs->zadd(0, "general_zset_key123", "test_field1", tm) == true);
    REQUIRE(rcs->zadd(0, "general_zset_key123", "test_field2", ++tm) == true);
    REQUIRE(rcs->zadd(0, "general_zset_key123", "test_field3", ++tm) == true);
    REQUIRE(rcs->zadd(0, "general_zset_key123", "test_field4", ++tm) == true);
    REQUIRE(rcs->zadd(0, "general_zset_key123", "test_field5", ++tm) == true);
    REQUIRE(rcs->zadd(0, "general_zset_key123", "test_field6", ++tm) == true);
    
    // getMemsByScoreWithLimit
    std::vector<ZSetMemberScore>  vecZsetList;
    REQUIRE(rcs->getMemsByScoreWithLimit(0, "general_zset_key123", 0, tm - 2, 0, 3, vecZsetList) == true);
    REQUIRE(vecZsetList.size() == 3);
    REQUIRE(rcs->getMemsByScoreWithLimit(0, "general_zset_key123", 0, tm - 2, 3, 3, vecZsetList) == true);
    REQUIRE(vecZsetList.size() == 4);

    REQUIRE(vecZsetList[0].member == "test_field1");
    REQUIRE(vecZsetList[0].score == tm - 5);

    REQUIRE(vecZsetList[1].member == "test_field2");
    REQUIRE(vecZsetList[1].score == tm - 4);
    REQUIRE(vecZsetList[2].member == "test_field3");
    REQUIRE(vecZsetList[2].score == tm - 3);
    REQUIRE(vecZsetList[3].member == "test_field4");
    REQUIRE(vecZsetList[3].score == tm - 2);
    
    // zrem
    std::vector<std::string>  vecDel;
    vecDel.push_back("test_field11");
    vecDel.push_back("test_field2");
    
    REQUIRE(rcs->zrem(0, "general_zset_key123", vecDel) == true);

    vecZsetList.clear();
    REQUIRE(rcs->getMemsByScoreWithLimit(0, "general_zset_key123", 0, tm - 1, 0, 6, vecZsetList) == true);
    REQUIRE(vecZsetList.size() == 4);
    
    REQUIRE(vecZsetList[0].member == "test_field1");
    REQUIRE(vecZsetList[0].score == tm - 5);
    REQUIRE(vecZsetList[1].member == "test_field3");
    REQUIRE(vecZsetList[1].score == tm - 3);
    REQUIRE(vecZsetList[2].member == "test_field4");
    REQUIRE(vecZsetList[2].score == tm - 2);
    REQUIRE(vecZsetList[3].member == "test_field5");
    REQUIRE(vecZsetList[3].score == tm - 1);

}