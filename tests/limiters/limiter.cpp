#include "../test_common.h"
#include "../../src/limiters/configuration_manager.h"
#include "../../src/limiters/limiter_config_update.h"
#include "../../src/redis/reply.h"
#include "../../src/utils/thread_utils.h"
#include "../../src/fiber/fiber_timer.h"
#include "../../src/limiters/api_qps_limiter.h"
#include "../../src/limiters/user_qps_limiter.h"
#include "../../src/limiters/limiter_globals.h"
#include "../../src/limiters/limiter_executor.h"
#include "../../src/limiters/distributed_limiter.h"
#include "../../src/limiters/dependency_limiter.h"
#include "../../src/utils/log.h"
#include "../../src/redis/redis_manager.h"
#include "metrics_client.h"
#include <chrono>

using namespace bcm;
using namespace bcm::dao;

class LimiterConfigurationsMock : public LimiterConfigurations {
public:
    virtual ErrorCode load(LimiterConfigs& configs)
    {
        configs = m_configs;
        return dao::ErrorCode::ERRORCODE_SUCCESS;
    }

    virtual ErrorCode get(const std::set<std::string>& keys, LimiterConfigs& configs)
    {
        for (const auto& k : keys) {
            auto it = m_configs.find(k);
            if (it != m_configs.end()) {
                configs.emplace(*it);
            }
        }
        return dao::ErrorCode::ERRORCODE_SUCCESS;
    }

    virtual ErrorCode set(const LimiterConfigs& configs)
    {
        m_configs.insert(configs.begin(), configs.end());
        return dao::ErrorCode::ERRORCODE_SUCCESS;
    }
private:
    LimiterConfigs m_configs;
};

static LimiterGlobals::ApiSet kIgnoredApis = {
    Api(http::verb::get, "/v1/accounts/sms/verification_code/:phonenumber"),
    Api(http::verb::get, "/v1/accounts/bind_phonenumber/:phonenumber/:verification_code"),
    Api(http::verb::get, "/v1/accounts/unbind_phonenumber"),
    Api(http::verb::put, "/v1/accounts/bind_phonenumber"),
    Api(http::verb::put, "/v1/contacts"),
    Api(http::verb::get, "/v1/contacts/token/:token"),
    Api(http::verb::put, "/v1/contacts/tokens"),
    Api(http::verb::put, "/v2/contacts/tokens"),
    Api(http::verb::get, "/v2/contacts/tokens/users"),
    Api(http::verb::post, "/v1/group/deliver/query_uids"),
    Api(http::verb::get, "/v1/keepalive/provisioning"),
    Api(http::verb::get, "/v1/keepalive")
};

TEST_CASE("LimiterConfigurationManager")
{
    LimiterConfigurationManager* manager = LimiterConfigurationManager::getInstance();
    manager->m_configDao = std::make_shared<LimiterConfigurationsMock>();
    manager->initialize();
    REQUIRE(manager->m_configs.empty() == true);
    LimiterConfigurations::LimiterConfigs configs;
    configs.emplace("test", LimitRule(1, 10));
    manager->m_configDao->set(configs);
    
    auto fiberTimer = new FiberTimer();
    std::shared_ptr<LimiterConfigUpdate> limiterConfigUpdater = std::make_shared<LimiterConfigUpdate>();
    fiberTimer->schedule(limiterConfigUpdater, 2 * 1000, false);

    std::this_thread::sleep_for(std::chrono::seconds(5));

    REQUIRE(manager->m_configs.size() == 1);
    LimitRule r1 = manager->m_configs["test"];
    LimitRule r2 = configs["test"];
    REQUIRE(r1.period == r2.period);
    REQUIRE(r1.count == r2.count);
    fiberTimer->clear();
    //delete fiberTimer;
    manager->uninitialize();
}

TEST_CASE("ApiMatcher")
{
    ApiMatcher matcher;
    std::vector<Api> patterns = {
        Api(http::verb::put, "/v1/attachments/upload/:attachmentId"),
        Api(http::verb::post, "/v1/group/deliver/is_qr_code_valid"), 
        Api(http::verb::get, "/v1/accounts/bind_phonenumber/:phonenumber/:verification_code")
    };

    std::vector<Api> targets = {
        Api(http::verb::put, "/v1/attachments/upload/123456"),
        Api(http::verb::post, "/v1/group/deliver/is_qr_code_valid"), 
        Api(http::verb::get, "/v1/accounts/bind_phonenumber/12345/67890")
    };

    for (size_t i = 0; i < patterns.size(); i++) {
        REQUIRE(matcher.match(patterns[i], targets[i]) == true);
    }

    std::vector<Api> tooManys = {
        Api(http::verb::put, "/v1/attachments/upload/123456/12345"),
        Api(http::verb::post, "/v1/group/deliver/is_qr_code_valid/123456"), 
        Api(http::verb::get, "/v1/accounts/bind_phonenumber/12345/67890/12345")
    };

    for (size_t i = 0; i < patterns.size(); i++) {
        REQUIRE(matcher.match(patterns[i], tooManys[i]) == false);
    }

    std::vector<Api> invalids = {
        Api(http::verb::put, "/v1/attachments/supload/123456"),
        Api(http::verb::post, "/v1/groups/deliver/is_qr_code_valid"), 
        Api(http::verb::get, "/v2/accounts/bind_phonenumber/12345/67890")
    };

    for (size_t i = 0; i < patterns.size(); i++) {
        REQUIRE(matcher.match(patterns[i], invalids[i]) == false);
    }

    std::string val;
    REQUIRE(matcher.matchAndFetch(patterns[0], targets[0], ":attachmentId", val) == true);
    REQUIRE(val == "123456");
    REQUIRE(matcher.matchAndFetch(patterns[2], targets[2], ":phonenumber", val) == true);
    REQUIRE(val == "12345");
    REQUIRE(matcher.matchAndFetch(patterns[2], targets[2], ":verification_code", val) == true);
    REQUIRE(val == "67890");
}

TEST_CASE("ApiSet")
{
    ApiMatcher matcher;
    LimiterGlobals::ApiSet patterns = {
        Api(http::verb::get, "/v1/profile/id"),
        Api(http::verb::get, "/v1/profile/:uid"),
        Api(http::verb::get, "/v1/profile/download/:avatarId"),
        Api(http::verb::get, "/v1/profile/keys")
    };

    Api api(http::verb::get, "/v1/profile/id");
    for (const auto& item : patterns) {
        if (matcher.match(item, api)) {
            REQUIRE(item.name == api.name);
            break;
        }
    }
    api.name = "/v1/profile/keys";
    for (const auto& item : patterns) {
        if (matcher.match(item, api)) {
            REQUIRE(item.name == api.name);
            break;
        }
    }
    api.name = "/v1/profile/12345";
    for (const auto& item : patterns) {
        if (matcher.match(item, api)) {
            REQUIRE(item.name == "/v1/profile/:uid");
            break;
        }
    }
}

TEST_CASE("ApiQpsLimiter")
{
    bcm::metrics::MetricsConfig config;
    config.appVersion = "1.0";
    config.reportQueueSize = 5000;
    config.metricsDir = "/tmp";
    config.metricsFileSizeInBytes = 1024*10;
    config.metricsFileCount = 5;
    config.reportIntervalInMs = 3000;
    config.clientId = "00001";
    config.writeThresholdInBytes = 1024 * 1024;
    bcm::metrics::MetricsClient::Init(config);
    std::shared_ptr<IApiMatcher> matcher = std::make_shared<ApiMatcher>();
    ApiQpsLimiter limiter(matcher);
    auto ruleKey = [](const Api& api) -> std::string {
        std::string res = bcm::to_string(api);
        if (res.empty()) {
            return "";
        }
        if (res[0] == '/') {
            return std::string(res.c_str() + 1, res.c_str() + res.size());
        }
        return res;
    };

    int64_t period = 1000;
    int64_t count = 10;
    int64_t defaultCount = 5;
    limiter.m_rules.clear();
    limiter.m_rules.emplace(kDefaultApiQpsRuleKey, LimitRule(period, defaultCount));
    for (const auto& item : LimiterGlobals::kActiveApis) {
        limiter.m_rules.emplace(ruleKey(item), LimitRule(period, count));
    }
    std::map<std::string, dao::LimitRule> backup = limiter.m_rules;
    for (const auto& item : LimiterGlobals::kActiveApis) {
        for (int i = 0; i < count; i++) {
            REQUIRE(limiter.acquireAccess(bcm::to_string(item)) == LimitLevel::GOOD);
        }
        REQUIRE(limiter.acquireAccess(bcm::to_string(item)) == LimitLevel::LIMITED);
        REQUIRE(limiter.m_status.find(ruleKey(item))->second.status.counter == count + 1);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2 * period));
    for (const auto& item : LimiterGlobals::kActiveApis) {
        REQUIRE(limiter.acquireAccess(bcm::to_string(item)) == LimitLevel::GOOD);
        REQUIRE(limiter.m_status.find(ruleKey(item))->second.status.counter == 1);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2 * period));
    for (const auto& item : LimiterGlobals::kActiveApis) {
        limiter.m_rules = backup;
        limiter.m_rules.erase(ruleKey(item));
        for (int i = 0; i < defaultCount; i++) {
            REQUIRE(limiter.acquireAccess(bcm::to_string(item)) == LimitLevel::GOOD);
        }
        REQUIRE(limiter.acquireAccess(bcm::to_string(item)) == LimitLevel::LIMITED);
        REQUIRE(limiter.m_status.find(ruleKey(item))->second.status.counter == defaultCount + 1);
    }
    limiter.m_rules.clear();
    limiter.m_rules.emplace(kDefaultApiQpsRuleKey, LimitRule(period, defaultCount));
    std::this_thread::sleep_for(std::chrono::milliseconds(2 * period));
    for (const auto& item : LimiterGlobals::kActiveApis) {
        REQUIRE(limiter.acquireAccess(bcm::to_string(item)) == LimitLevel::GOOD);
        REQUIRE(limiter.m_status.find(ruleKey(item))->second.status.counter == 1);
    }
}

TEST_CASE("UserQpsLimiter")
{
    bcm::metrics::MetricsConfig config;
    config.appVersion = "1.0";
    config.reportQueueSize = 5000;
    config.metricsDir = "/tmp";
    config.metricsFileSizeInBytes = 1024*10;
    config.metricsFileCount = 5;
    config.reportIntervalInMs = 3000;
    config.clientId = "00001";
    config.writeThresholdInBytes = 1024 * 1024;
    bcm::metrics::MetricsClient::Init(config);
    UserQpsLimiter limiter;
    int64_t period = 1000;
    int64_t count = 10;
    limiter.m_rule.period = period;
    limiter.m_rule.count = count;
    std::set<std::string> uids = {
        "18CjwL8cb1aDNMgCUejhbpZc5yNnuR18vR",
        "1FZ9cDd6Mqq15m7QFwq5C1mkatyk1ujcJL",
        "13bYv8XkhgT8VjDrrekJDMR9QqeWWBdLiV",
        "1GtrnFtaasT1NWsUb5sJW945EP25diRyvw",
        "1LzrzEE63hnoKW7syyKU42naGvUtg4UvGx"
    };
    for (const auto& uid : uids) {
        for (int i = 0; i < count; i++) {
            REQUIRE(limiter.acquireAccess(uid) == LimitLevel::GOOD);
        }
        REQUIRE(limiter.acquireAccess(uid) == LimitLevel::LIMITED);
        REQUIRE(limiter.m_status.find(uid)->second.status.counter == count + 1);
    }
}

TEST_CASE("DistributedLimiter")
{
    bcm::metrics::MetricsConfig config;
    config.appVersion = "1.0";
    config.reportQueueSize = 5000;
    config.metricsDir = "/tmp";
    config.metricsFileSizeInBytes = 1024*10;
    config.metricsFileCount = 5;
    config.reportIntervalInMs = 3000;
    config.clientId = "00001";
    config.writeThresholdInBytes = 1024 * 1024;
    bcm::metrics::MetricsClient::Init(config);
    bcm::RedisConfig redisCfg;
    redisCfg.ip = "127.0.0.1";
    redisCfg.port = 6379;    
    redisCfg.password = "";
    redisCfg.regkey = "";

    std::unordered_map<std::string, std::unordered_map<std::string, RedisConfig> > configs;
    configs["p0"]["0"] = redisCfg;
    configs["p0"]["1"] = redisCfg;

    RedisDbManager::Instance()->setRedisDbConfig(configs);
    static const std::string kGroupCreationLimiterName = "GroupCreationLimiter";
    static const std::string kGroupCreationConfigKey = "special/group_creation";
    int64_t period = 10 * 1000;
    int64_t count = 5;
    DistributedLimiter limiter(kGroupCreationLimiterName, kGroupCreationConfigKey, period, count);
    limiter.m_rule.period = period;
    limiter.m_rule.count = count;

    std::set<std::string> uids = {
        "18CjwL8cb1aDNMgCUejhbpZc5yNnuR18vR",
        "1FZ9cDd6Mqq15m7QFwq5C1mkatyk1ujcJL",
        "13bYv8XkhgT8VjDrrekJDMR9QqeWWBdLiV",
        "1GtrnFtaasT1NWsUb5sJW945EP25diRyvw",
        "1LzrzEE63hnoKW7syyKU42naGvUtg4UvGx"
    };
    for (const auto& uid : uids) {
        for (int i = 0; i < count; i++) {
            REQUIRE(limiter.acquireAccess(uid) == LimitLevel::GOOD);
        }
        REQUIRE(limiter.acquireAccess(uid) == LimitLevel::LIMITED);
        REQUIRE(limiter.m_status.find(uid)->second.count == count + 1);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(period * 2));

    for (const auto& uid : uids) {
        for (int i = 0; i < count; i++) {
            REQUIRE(limiter.acquireAccess(uid) == LimitLevel::GOOD);
        }
        REQUIRE(limiter.acquireAccess(uid) == LimitLevel::LIMITED);
        REQUIRE(limiter.m_status.find(uid)->second.count == count + 1);
    }
}

TEST_CASE("DependencyLimiter")
{
    bcm::metrics::MetricsConfig config;
    config.appVersion = "1.0";
    config.reportQueueSize = 5000;
    config.metricsDir = "/tmp";
    config.metricsFileSizeInBytes = 1024*10;
    config.metricsFileCount = 5;
    config.reportIntervalInMs = 3000;
    config.clientId = "00001";
    config.writeThresholdInBytes = 1024 * 1024;
    bcm::metrics::MetricsClient::Init(config);
    bcm::RedisConfig redisCfg;
    redisCfg.ip = "127.0.0.1";
    redisCfg.port = 6379;    
    redisCfg.password = "";
    redisCfg.regkey = "";

    std::unordered_map<std::string, std::unordered_map<std::string, RedisConfig> > configs;
    configs["p0"]["0"] = redisCfg;
    configs["p0"]["1"] = redisCfg;

    RedisDbManager::Instance()->setRedisDbConfig(configs);
    static const std::string kGroupCreationLimiterName = "GroupCreationLimiter";
    static const std::string kGroupCreationConfigKey = "special/group_creation";
    int64_t period = 10 * 1000;
    int64_t count = 5;
    std::shared_ptr<DistributedLimiter> limiter = std::make_shared<DistributedLimiter>(kGroupCreationLimiterName, 
                                                                                       kGroupCreationConfigKey, 
                                                                                       period, 
                                                                                       count);
    limiter->m_rule.period = period;
    limiter->m_rule.count = count;

    std::set<std::string> uids = {
        "28CjwL8cb1aDNMgCUejhbpZc5yNnuR18vR",
        "2FZ9cDd6Mqq15m7QFwq5C1mkatyk1ujcJL",
        "23bYv8XkhgT8VjDrrekJDMR9QqeWWBdLiV",
        "2GtrnFtaasT1NWsUb5sJW945EP25diRyvw",
        "2LzrzEE63hnoKW7syyKU42naGvUtg4UvGx"
    };
    for (const auto& uid : uids) {
        for (int i = 0; i < count; i++) {
            REQUIRE(limiter->acquireAccess(uid) == LimitLevel::GOOD);
        }
        REQUIRE(limiter->acquireAccess(uid) == LimitLevel::LIMITED);
        REQUIRE(limiter->m_status.find(uid)->second.count == count + 1);
    }

    static const std::string kDhKeysLimiterName = "DhKeysLimiter";
    static const std::string kDhKeysConfigKey = "special/dh_keys";
    std::vector<std::shared_ptr<ILimiter>> dependencies;
    dependencies.emplace_back(limiter);
    std::shared_ptr<DistributedLimiter> l(new DistributedLimiter(kDhKeysLimiterName,
                                                                 kDhKeysConfigKey,
                                                                 period,
                                                                 count));
    DependencyLimiter dependencyLimiter(l, dependencies);
    for (const auto& uid : uids) {
        REQUIRE(dependencyLimiter.acquireAccess(uid) == LimitLevel::LIMITED);
        REQUIRE(l->m_status.find(uid) == l->m_status.end());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(period * 2));

    for (const auto& uid : uids) {
        for (int i = 0; i < count; i++) {
            REQUIRE(limiter->acquireAccess(uid) == LimitLevel::GOOD);
            REQUIRE(dependencyLimiter.acquireAccess(uid) == LimitLevel::GOOD);
        }
        REQUIRE(dependencyLimiter.acquireAccess(uid) == LimitLevel::LIMITED);
        REQUIRE(l->m_status.find(uid)->second.count == count + 1);
    }
}

TEST_CASE("LimiterExector")
{
    LimiterGlobals::getInstance()->m_ignoredApis = kIgnoredApis;
    LimiterExecutor executor;
    for (const auto& item : LimiterGlobals::kActiveApis) {
        LOGI << "api: " << bcm::to_string(item);
        REQUIRE(executor.ignore(item) == false);
    }
    for (const auto& item : kIgnoredApis) {
        LOGI << "api: " << bcm::to_string(item);
        REQUIRE(executor.ignore(item) == true);
    }
}