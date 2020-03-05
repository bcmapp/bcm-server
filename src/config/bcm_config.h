#pragma once

#include "log_config.h"
#include "service_config.h"
#include "dao_config.h"
#include "turn_config.h"
#include "redis_config.h"
#include "apns_config.h"
#include "fcm_config.h"
#include "umeng_config.h"
#include "lbs_config.h"
#include "challenge_config.h"
#include "dispatcher_config.h"
#include "encrypt_sender.h"
#include "noise_config.h"
#include "bcm_metrics_config.h"
#include "size_check_config.h"
#include "limiters_config.h"
#include "s3_config.h"
#include "group_config.h"
#include "cache_config.h"
#include "multi_device_config.h"

namespace bcm {

struct BcmConfig {
    LogConfig log;
    ServiceConfig http;
    DaoConfig dao;
    TurnConfig turn;
    std::vector<RedisConfig> redis;    // redis for pub/sub
    std::unordered_map<std::string, std::unordered_map<std::string, RedisConfig> > groupRedis; // redis for group info
    std::map<std::string, std::vector<RedisConfig>> onlineRedis;
    LbsConfig lbs;
    ChallengeConfig challenge;
    DispatcherConfig dispatcher;
    EncryptSenderConfig encryptSender;
    NoiseConfig noise;
    BCMMetricsConfig bcmMetricsConfig;
    SizeCheckConfig sizeCheck;
    LimiterConfig limiterConfig;
    S3Config s3Config;
    GroupConfig groupConfig;
    CacheConfig cacheConfig;
    MultiDeviceConfig multiDeviceConfig;
};

inline void to_json(nlohmann::json& j, const BcmConfig& config)
{
    j = nlohmann::json{{"log", config.log},
                       {"http", config.http},
                       {"dao", config.dao},
                       {"turn", config.turn},
                       {"redis", config.redis},
                       {"groupRedis", config.groupRedis},
                       {"onlineRedis", config.onlineRedis},
                       {"lbs", config.lbs},
                       {"challenge", config.challenge},
                       {"dispatcher", config.dispatcher},
                       {"encryptSender", config.encryptSender},
                       {"noise", config.noise},
                       {"metrics", config.bcmMetricsConfig},
                       {"sizeCheck", config.sizeCheck},
                       {"limitConfig", config.limiterConfig},
                       {"s3Config", config.s3Config},
                       {"groupConfig", config.groupConfig},
                       {"multiDevice", config.multiDeviceConfig},
                       {"cache", config.cacheConfig}};

}

inline void from_json(const nlohmann::json& j, BcmConfig& config)
{
    jsonable::toGeneric(j, "log", config.log);
    jsonable::toGeneric(j, "http", config.http);
    jsonable::toGeneric(j, "dao", config.dao);
    jsonable::toGeneric(j, "turn", config.turn);
    jsonable::toGeneric(j, "redis", config.redis);
    jsonable::toGeneric(j, "groupRedis", config.groupRedis);
    jsonable::toGeneric(j, "onlineRedis", config.onlineRedis);
    jsonable::toGeneric(j, "lbs", config.lbs);
    jsonable::toGeneric(j, "metrics", config.bcmMetricsConfig);
    jsonable::toGeneric(j, "challenge", config.challenge, jsonable::OPTIONAL);
    jsonable::toGeneric(j, "dispatcher", config.dispatcher, jsonable::OPTIONAL);
    jsonable::toGeneric(j, "encryptSender", config.encryptSender, jsonable::OPTIONAL);
    jsonable::toGeneric(j, "noise", config.noise, jsonable::OPTIONAL);
    jsonable::toGeneric(j, "sizeCheck", config.sizeCheck, jsonable::OPTIONAL);
    jsonable::toGeneric(j, "limiterConfig", config.limiterConfig, jsonable::OPTIONAL);
    jsonable::toGeneric(j, "s3Config", config.s3Config);
    jsonable::toGeneric(j, "groupConfig", config.groupConfig);
    jsonable::toGeneric(j, "cache", config.cacheConfig, jsonable::OPTIONAL);
    jsonable::toGeneric(j, "multiDevice", config.multiDeviceConfig, jsonable::OPTIONAL);
}

}
