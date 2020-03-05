#include "user_qps_limiter.h"
#include "utils/time.h"
#include "utils/log.h"
#include "limiter_globals.h"
#include "metrics_client.h"

namespace bcm {

LimitLevel UserQpsLimiter::acquireAccess(const std::string& id) {
    int64_t now = steadyNowInMilli();
    dao::LimitRule rule(0, 0);
    // share lock here because we just read m_rule
    {
        boost::shared_lock<boost::shared_mutex> guard(m_ruleMutex);
        rule = m_rule;
    }
    // share lock to get the status that corresponding to id, if it's not existed
    // then we go to exclusive lock to insert a new item for it
    Item* item = nullptr;
    {
        boost::shared_lock<boost::shared_mutex> guard(m_statusMutex);
        auto it = m_status.find(id);
        if (it != m_status.end()) {
            item = &(it->second);
        }
    }
    if (item == nullptr) {
        boost::unique_lock<boost::shared_mutex> guard(m_statusMutex);
        auto it = m_status.emplace(id, Item());
        item = &(it.first->second);
    }
    // here we use exclusive lock on uid level because there won't be too much
    // requests from the same uid at the same time
    int64_t counter = 0;
    {
        boost::unique_lock<boost::shared_mutex> guard(*(item->mutex));
        auto& status = item->status;
        if (status.startTime == 0 || now - status.startTime >= rule.period) {
            status.startTime = now;
            status.counter = 1;
            LOGT << "limiter status - uid: " << id << ", counter: " << status.counter << ", rule.count: " << rule.count;;
            return LimitLevel::GOOD;
        }
        status.counter++;
        if (status.counter <= rule.count) {
            LOGT << "limiter status - uid: " << id << ", counter: " << status.counter << ", rule.count: " << rule.count;;
            return LimitLevel::GOOD;
        }
        counter = status.counter;
    }

    bcm::metrics::MetricsClient::Instance()->markMicrosecondAndRetCode(LimiterGlobals::kLimiterServiceName,
                                                                       kIdentity,
                                                                       0,
                                                                       static_cast<LimitLevel>(LimitLevel::LIMITED));
    LOGE << "limiter rejected, status - uid: " << id << ", counter: " << counter << ", rule.count: " << rule.count;;
    return LimitLevel::LIMITED;
}

LimitLevel UserQpsLimiter::limited(const std::string& id) {
    Status s;
    {
        boost::shared_lock<boost::shared_mutex> guard(m_statusMutex);
        auto it = m_status.find(id);
        if (it == m_status.end()) {
            return LimitLevel::GOOD;
        }
        s = it->second.status;
    }
    dao::LimitRule rule = m_rule;
    int64_t now = steadyNowInMilli();
    if (s.counter > rule.count && now - s.startTime < rule.period) {
        return LimitLevel::LIMITED;
    }
    return LimitLevel::GOOD;
}

void UserQpsLimiter::currentState(LimitState& state) {
    if (state.keys.empty()) {
        return;
    }

    auto& result = state.counters;
    {
        boost::shared_lock<boost::shared_mutex> guard(m_statusMutex);
        for (const auto& k : state.keys) {
            auto it = m_status.find(k);
            if (it != m_status.end()) {
                result.emplace(it->first, it->second.status.counter);
            }
        }
    }
    state.id = kIdentity;
}

void UserQpsLimiter::update(const LimiterConfigUpdateEvent& event) {
    auto it = event.configs.find(kUserQpsRuleKey);
    if (it == event.configs.end()) {
        if (event.removed.find(kUserQpsRuleKey) != event.removed.end()) {
            boost::unique_lock<boost::shared_mutex> guard(m_ruleMutex);
            m_rule.count = 1000;
            m_rule.period = 1000;
        }
        return;
    }
    {
        boost::unique_lock<boost::shared_mutex> guard(m_ruleMutex);
        if (m_rule.count != it->second.count || m_rule.period != it->second.period) {
            m_rule = it->second;
        }
    }
}

const std::string UserQpsLimiter::kIdentity = "UserQpsLimiter";

}    // namespace bcm