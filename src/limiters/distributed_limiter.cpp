#include "distributed_limiter.h"
#include "limiter_globals.h"
#include "utils/time.h"
#include "utils/log.h"
#include "metrics_client.h"
#include "redis/hiredis_client.h"
#include "redis/redis_manager.h"

namespace bcm {

LimitLevel DistributedLimiter::acquireAccess(const std::string& id) {
    std::map<std::string, dao::LimitRule> rules;
    // share lock here because we just read m_rules
    const std::string& uid = id;
    
    dao::LimitRule rule;
    {
        boost::shared_lock<boost::shared_mutex> guard(m_ruleMutex);
        rule = m_rule;
    }

    //int64_t now = nowInMilli();
    int64_t timeSlot = nowInMilli() / rule.period;
    // share lock to get the status that corresponding to the uid, if it's not existed
    // then we go to exclusive lock to insert a new item for it
    Item* item = nullptr;
    bool limited = false;
    {
        boost::shared_lock<boost::shared_mutex> guard(m_statusMutex);
        auto it = m_status.find(uid);
        if (it != m_status.end()) {
            item = &(it->second);
            if (timeSlot == item->timeSlot && item->count > rule.count) {
                limited = true;
            }
        }
    }
    if (item == nullptr) {
        boost::unique_lock<boost::shared_mutex> guard(m_statusMutex);
        auto it = m_status.emplace(id, Item());
        item = &(it.first->second);
    }

    if (limited) {
        {
            boost::unique_lock<boost::shared_mutex> guard(*(item->mutex));
            item->count++;
        }
        LOGE << "limiter rejected, " << m_identity << " status - uid: " << uid 
             << ", timeSlot: " << item->timeSlot << ", counter: " << item->count << ", rule.count: " << rule.count;
        bcm::metrics::MetricsClient::Instance()->markMicrosecondAndRetCode(LimiterGlobals::kLimiterServiceName,
                                                                           m_identity,
                                                                           0,
                                                                           static_cast<LimitLevel>(LimitLevel::LIMITED));
        return LimitLevel::LIMITED;
    }

    int64_t count = 0;
    incr(uid, rule, count, timeSlot);
    {
        boost::unique_lock<boost::shared_mutex> guard(*(item->mutex));
        item->count = count;
        item->timeSlot = timeSlot;
    }

    LOGT << "limiter status, " << m_identity << " - uid: " << uid 
         << ", timeSlot: " << timeSlot << ", counter: " << count << ", rule.count: " << rule.count;

    if (count > rule.count) {
        bcm::metrics::MetricsClient::Instance()->markMicrosecondAndRetCode(LimiterGlobals::kLimiterServiceName, 
                                                                           m_identity, 
                                                                           0,
                                                                           static_cast<LimitLevel>(LimitLevel::LIMITED));
        LOGE << m_identity << " limiter status - uid: " << uid << ", timeSlot: " << timeSlot
             << ", counter: " << count << ", rule.count: " << rule.count;
        return LimitLevel::LIMITED;
    }

    return LimitLevel::GOOD;
}

LimitLevel DistributedLimiter::limited(const std::string& id) {
    Item s;
    {
        boost::shared_lock<boost::shared_mutex> guard(m_statusMutex);
        auto its = m_status.find(id);
        if (its == m_status.end()) {
            return LimitLevel::GOOD;
        }
        s = its->second;
    }
    dao::LimitRule rule = m_rule;
    int64_t timeSlot = nowInMilli() / rule.period;
    if (timeSlot == s.timeSlot && s.count > rule.count) {
        return LimitLevel::LIMITED;
    }
    return LimitLevel::GOOD;
}

void DistributedLimiter::currentState(LimitState& state) {
    if (state.keys.empty()) {
        return;
    }

    auto& result = state.counters;
    {
        boost::shared_lock<boost::shared_mutex> guard(m_statusMutex);
        for (const auto& k : state.keys) {
            auto it = m_status.find(k);
            if (it != m_status.end()) {
                result.emplace(it->first, it->second.count);
            }
        }
    }
    state.id = m_identity;
}

void DistributedLimiter::update(const LimiterConfigUpdateEvent& event) {
    auto it = event.configs.find(m_configKey);
    if (it == event.configs.end()) {
        if (event.removed.find(m_configKey) != event.removed.end()) {
            boost::unique_lock<boost::shared_mutex> guard(m_ruleMutex);
            m_rule.count = m_defaultCount;
            m_rule.period = m_defaultPeriod;
        }
        return;
    }
    if (it->second.period < 1000) {
        LOGW << "period can not be less than 1000, value: " << it->second.period;
        return;
    }
    {
        boost::unique_lock<boost::shared_mutex> guard(m_ruleMutex);
        if (m_rule.period != it->second.period
            || m_rule.count != it->second.count)
        m_rule = it->second;
    }
}

void DistributedLimiter::incr(const std::string& uid, 
                              const dao::LimitRule& rule,
                              int64_t& count,
                              int64_t timeSlot)
{
    uint64_t new_value = 0;
    uint32_t period = rule.period / 1000; // period in seconds
    std::ostringstream oss;
    oss << m_identity << "_" << uid << "_" << timeSlot;
    std::string keyId = oss.str();
    int32_t ret = RedisDbManager::Instance()->incr(uid, keyId, new_value);
    
    // Communication error
    auto communicationErrorHandler = [&]() {
        LOGE << "failed to incr: (" << m_identity << "," << keyId << ")." ;
    };

    // Key already exist but in invalid type, update it explicitly
    auto invalidKeyTypeHandler = [&]() {
        LOGE << "unexpeted type for (" << m_identity << "," << keyId << ")." ;
        RedisDbManager::Instance()->set(uid, keyId, "1");
        RedisDbManager::Instance()->expire(uid, keyId, period);
    };

    // Normal valid reply
    //     case 1: a new interval, set expire time
    //     case 2: exceed threshold, do nothing and wait redis to expire
    //     case 3: within the threshold, do nothing
    auto normalReplyHandler = [&]() {
        if (1 == new_value) {
            RedisDbManager::Instance()->expire(uid, keyId, period);
        }
    };     

    switch (ret) {
        case -1:
            communicationErrorHandler();
            count = 0;
        case 0:
            invalidKeyTypeHandler();
            count = 1;
        default:
            normalReplyHandler();
            count = new_value;
    }
}

}
