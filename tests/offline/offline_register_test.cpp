#include "../test_common.h"

#include <thread>
#include <chrono>

#include <hiredis/hiredis.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <boost/core/ignore_unused.hpp>

#include <utils/jsonable.h>
#include <utils/time.h>
#include <utils/log.h>
#include "utils/libevent_utils.h"
#include "utils/thread_utils.h"

#include "redis/async_conn.h"
#include "redis/reply.h"

#include "../../src/fiber/fiber_timer.h"
#include "../../src/registers/offline_register.h"

#include "../../src/config/redis_config.h"
#include "../../src/config/service_config.h"
#include "../../src/config/offline_server_config.h"
#include "../../src/dao/client.h"


using namespace bcm;
using namespace bcm::dao;

TEST_CASE("offlineRegisterTest")
{
    evthread_use_pthreads();

    LogConfig log;
    log.level = 0;
    log.directory = "./";
    log.console = true;
    log.autoflush = true;
    
    Log::init(log, "offline-server");

    bcm::RedisConfig redisCfg = {"127.0.0.1", 6379, "", ""};
    bcm::ServiceConfig  srvCfg;
    srvCfg.host = "127.0.0.1";
    srvCfg.port = 9001;
    srvCfg.ips.push_back("127.0.0.1");

    std::vector<std::string> vecPushType;
    vecPushType.emplace_back(TYPE_SYSTEM_PUSH_APNS);

    auto fiberTimer = std::make_shared<FiberTimer>();
    
    // test1
    auto offlineServiceRegister1 = std::make_shared<OfflineServiceRegister>(redisCfg,
                                                                           srvCfg, vecPushType);

    fiberTimer->schedule(offlineServiceRegister1, OfflineServiceRegister::kKeepAliveInterval, false);
    
    // test2
    srvCfg.port = 9002;
    vecPushType.clear();
    vecPushType.emplace_back(TYPE_SYSTEM_PUSH_UMENG);
    auto offlineServiceRegister2 = std::make_shared<OfflineServiceRegister>(redisCfg,
                                                                            srvCfg, vecPushType);

    fiberTimer->schedule(offlineServiceRegister2, OfflineServiceRegister::kKeepAliveInterval, false);

    // test3
    srvCfg.port = 9003;
    vecPushType.clear();
    vecPushType.emplace_back(TYPE_SYSTEM_PUSH_FCM);
    auto offlineServiceRegister3 = std::make_shared<OfflineServiceRegister>(redisCfg,
                                                                            srvCfg, vecPushType);
    fiberTimer->schedule(offlineServiceRegister3, OfflineServiceRegister::kKeepAliveInterval, false);

    // test4
    srvCfg.port = 9004;
    vecPushType.clear();
    vecPushType.emplace_back(TYPE_SYSTEM_PUSH_APNS);
    vecPushType.emplace_back(TYPE_SYSTEM_PUSH_UMENG);
    vecPushType.emplace_back(TYPE_SYSTEM_PUSH_FCM);
    auto offlineServiceRegister4 = std::make_shared<OfflineServiceRegister>(redisCfg,
                                                                            srvCfg, vecPushType);
    fiberTimer->schedule(offlineServiceRegister4, OfflineServiceRegister::kKeepAliveInterval, false);
    
    sleep(20);
    std::string serverIp1 = offlineServiceRegister1->getRandomOfflineServerByType(TYPE_SYSTEM_PUSH_APNS);
    std::string serverIp2 = offlineServiceRegister1->getRandomOfflineServerByType(TYPE_SYSTEM_PUSH_APNS);
    REQUIRE(serverIp1 != serverIp2);
    if (serverIp1 != "127.0.0.1:9001" && serverIp1 != "127.0.0.1:9004") {
        REQUIRE(serverIp1 == "127.0.0.1:9001");
    }
    if (serverIp2 != "127.0.0.1:9001" && serverIp2 != "127.0.0.1:9004") {
        REQUIRE(serverIp2 == "127.0.0.1:9001");
    }

    TLOG << "test type: " << TYPE_SYSTEM_PUSH_APNS << " serverIp1: " << serverIp1 << ", serverIp2: " << serverIp2;
    
    serverIp1 = offlineServiceRegister1->getRandomOfflineServerByType(TYPE_SYSTEM_PUSH_UMENG);
    serverIp2 = offlineServiceRegister1->getRandomOfflineServerByType(TYPE_SYSTEM_PUSH_UMENG);
    REQUIRE(serverIp1 != serverIp2);
    if (serverIp1 != "127.0.0.1:9002" && serverIp1 != "127.0.0.1:9004") {
        REQUIRE(serverIp1 == "127.0.0.1:9002");
    }
    if (serverIp2 != "127.0.0.1:9002" && serverIp2 != "127.0.0.1:9004") {
        REQUIRE(serverIp2 == "127.0.0.1:9002");
    }
    TLOG << "test type: " << TYPE_SYSTEM_PUSH_UMENG << " serverIp1: " << serverIp1 << ", serverIp2: " << serverIp2;

    serverIp1 = offlineServiceRegister1->getRandomOfflineServerByType(TYPE_SYSTEM_PUSH_FCM);
    serverIp2 = offlineServiceRegister1->getRandomOfflineServerByType(TYPE_SYSTEM_PUSH_FCM);
    REQUIRE(serverIp1 != serverIp2);
    if (serverIp1 != "127.0.0.1:9003" && serverIp1 != "127.0.0.1:9004") {
        REQUIRE(serverIp1 == "127.0.0.1:9002");
    }
    if (serverIp2 != "127.0.0.1:9003" && serverIp2 != "127.0.0.1:9004") {
        REQUIRE(serverIp2 == "127.0.0.1:9003");
    }
    TLOG << "test type: " << TYPE_SYSTEM_PUSH_FCM << " serverIp1: " << serverIp1 << ", serverIp2: " << serverIp2;
    
    TLOG << "test finished ";
    
    fiberTimer->cancel(offlineServiceRegister1);
    fiberTimer->cancel(offlineServiceRegister2);
    fiberTimer->cancel(offlineServiceRegister3);
    fiberTimer->cancel(offlineServiceRegister4);
    
    sleep(1);
    
}
