#pragma once

#include <string>

#include "../../config/log_config.h"
#include "../../config/service_config.h"
#include "../../config/dao_config.h"
#include "../../config/redis_config.h"
#include "../../config/apns_config.h"
#include "../../config/fcm_config.h"
#include "../../config/umeng_config.h"
#include "../../config/offline_server_config.h"
#include "../../config/sysmsg_config.h"

namespace bcm {

struct OfflineConfig {
    LogConfig log;
    ServiceConfig http;
    DaoConfig dao;
    std::vector<RedisConfig> redis;     // redis pub/sub
    std::unordered_map<std::string, std::unordered_map<std::string, RedisConfig>> groupRedis; // redis for store
    ApnsConfig apns;
    FcmConfig fcm;
    UmengConfig umeng;
    OfflineServerConfig offlineSvr;
    SysMsgConfig sysmsg;
};

inline void to_json(nlohmann::json& j, const OfflineConfig& config)
{
    j = nlohmann::json{{"log", config.log},
                       {"http", config.http},
                       {"dao", config.dao},
                       {"redis", config.redis},
                       {"groupRedis", config.groupRedis},
                       {"apns", config.apns},
                       {"fcm", config.fcm},
                       {"umeng", config.umeng},
                       {"offlineSvr", config.offlineSvr},
                       {"sysmsg", config.sysmsg}
        };
}

inline void from_json(const nlohmann::json& j, OfflineConfig& config)
{
    jsonable::toGeneric(j, "log", config.log);
    jsonable::toGeneric(j, "http", config.http);
    jsonable::toGeneric(j, "dao", config.dao);
    jsonable::toGeneric(j, "redis", config.redis);
    jsonable::toGeneric(j, "groupRedis", config.groupRedis);
    jsonable::toGeneric(j, "apns", config.apns);
    jsonable::toGeneric(j, "fcm", config.fcm);
    jsonable::toGeneric(j, "umeng", config.umeng);
    jsonable::toGeneric(j, "offlineSvr", config.offlineSvr);
    jsonable::toGeneric(j, "sysmsg", config.sysmsg, jsonable::OPTIONAL);
}


class OfflineOptions {
public:
    static OfflineOptions parseCmd(int argc,char* argv[]);
    OfflineConfig& getConfig() { return m_config; }

private:
    void readConfig(std::string& configFile);

private:
    OfflineConfig m_config;
};

} // namespace bcm
