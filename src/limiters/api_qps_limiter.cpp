#include "api_qps_limiter.h"
#include "limiter_globals.h"
#include "utils/time.h"
#include "utils/log.h"
#include "metrics_client.h"

namespace bcm {

ApiQpsLimiter::ApiQpsLimiter(std::shared_ptr<IApiMatcher>& apiMatcher) : m_apiMatcher(apiMatcher)
{ 
    //m_rules.emplace(kDefaultApiQpsRuleKey, dao::LimitRule(1000, 1000));
    for (const auto& item : LimiterGlobals::getActiveApis()) {
        std::string api = bcm::to_string(item);
        if (api.empty()) {
            continue;
        }
        m_apiPatterns.emplace(api);
        if (api[0] == '/') {
            api.erase(0, 1);
        }
        m_status.emplace(std::move(api), Item());
    }
}

LimitLevel ApiQpsLimiter::acquireAccess(const std::string& id) {
    int64_t now = steadyNowInMilli();
    std::string api("");
    for (const auto& pattern : m_apiPatterns) {
        if (m_apiMatcher->match(pattern, id)) {
            // remove the first char '/'
            if (pattern[0] == '/') {
                api.assign(pattern.c_str() + 1, pattern.c_str() + pattern.size());
            } else {
                api = pattern;
            }
            break;
        }
    }
    if (api.empty()) {
        return LimitLevel::GOOD;
    }

    // share lock here because we just read m_rules
    dao::LimitRule rule(1000, 1000);
    {
        boost::shared_lock<boost::shared_mutex> guard(m_ruleMutex);
        auto it = m_rules.find(api);
        if (it == m_rules.end()) {
            it = m_rules.find(kDefaultApiQpsRuleKey);
        }
        if (it != m_rules.end()){
            rule = it->second;
        }
    }

    Item* item = nullptr;
    auto it = m_status.find(api);
    if (it != m_status.end()) {
        item = &(it->second);
    }
    if (item == nullptr) {
        return LimitLevel::GOOD;
    }

    int64_t counter = 0;
    {
        boost::unique_lock<boost::shared_mutex> guard(*(item->mutex));
        auto& status = item->status;
        if (status.startTime == 0 || now - status.startTime >= rule.period) {
            status.startTime = now;
            status.counter = 1;
            LOGT << "limiter status - api: " << api << ", counter: " << status.counter << ", rule.count: " << rule.count
                 << ", id: " << id;
            return LimitLevel::GOOD;
        }
        status.counter++;
        if (status.counter <= rule.count) {
            LOGT << "limiter status - api: " << api << ", counter: " << status.counter << ", rule.count: " << rule.count
                << ", id: " << id;
            return LimitLevel::GOOD;
        }
        counter = status.counter;
    }
    bcm::metrics::MetricsClient::Instance()->markMicrosecondAndRetCode(LimiterGlobals::kLimiterServiceName,
                                                                       kIdentity,
                                                                       0,
                                                                       static_cast<LimitLevel>(LimitLevel::LIMITED));
    LOGE << "limiter rejected, limiter status - api: " << api << ", counter: " << counter 
         << ", rule.count: " << rule.count << ", id: " << id;
    return LimitLevel::LIMITED;
}

LimitLevel ApiQpsLimiter::limited(const std::string& id) {
    Status s;
    {
        boost::shared_lock<boost::shared_mutex> guard(m_statusMutex);
        auto it = m_status.find(id);
        if (it == m_status.end()) {
            return LimitLevel::GOOD;
        }
        s = it->second.status;
    }
    dao::LimitRule rule(1000, 1000);
    {
        boost::shared_lock<boost::shared_mutex> guard(m_ruleMutex);
        auto it = m_rules.find(id);
        if (it == m_rules.end()) {
            it = m_rules.find(kDefaultApiQpsRuleKey);
        }
        if (it != m_rules.end()){
            rule = it->second;
        }
    }
    int64_t now = steadyNowInMilli();
    if (s.counter > rule.count && now - s.startTime < rule.period) {
        return LimitLevel::LIMITED;
    }
    return LimitLevel::GOOD;
}

void ApiQpsLimiter::currentState(LimitState& state) {
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

void ApiQpsLimiter::update(const LimiterConfigUpdateEvent& event) {
    dao::LimiterConfigurations::LimiterConfigs changed;
    std::set<std::string> removed;
    dao::LimiterConfigurations::LimiterConfigs rules;
    {
        boost::shared_lock<boost::shared_mutex> guard(m_ruleMutex);
        rules = m_rules;
    }
    auto equal = [](const dao::LimitRule& l, const dao::LimitRule& r) -> bool {
        return l.count == r.count && l.period == r.period;
    };
    for (const auto& item : event.configs) {
        if (item.first == kDefaultApiQpsRuleKey) {
            auto it = rules.find(item.first);
            if (it == rules.end() || !equal(it->second, item.second)) {
                changed.emplace(item.first, dao::LimitRule(item.second.period, item.second.count));
            }
            continue;
        }

        if (boost::starts_with(item.first, kApiQpsRuleKeyPrefix)) {
            std::string name(item.first.c_str() + kApiQpsRuleKeyPrefix.size(), item.first.c_str() + item.first.size());
            auto it = rules.find(name);
            if (it == rules.end() || !equal(it->second, item.second)) {
                changed.emplace(std::move(name), dao::LimitRule(item.second.period, item.second.count));
            }
        }
    }

    for (const auto& item : event.removed) {
        if (item == kDefaultApiQpsRuleKey) {
            removed.emplace(kDefaultApiQpsRuleKey);
        }
        if (boost::starts_with(item, kApiQpsRuleKeyPrefix)) {
            std::string name(item.c_str() + kApiQpsRuleKeyPrefix.size(), item.c_str() + item.size());
            if (rules.find(name) != rules.end()) {
                removed.emplace(name);
            }
        }
    }

    if (changed.empty() && removed.empty()) {
        return;
    }
    {
        boost::unique_lock<boost::shared_mutex> guard(m_ruleMutex);
        for (const auto& item : changed) {
            m_rules[item.first] = item.second;
        }
        for (const auto& item : removed) {
            m_rules.erase(item);
        }
    }
}

const std::string ApiQpsLimiter::kIdentity = "ApiQpsLimiter";

}    // namespace bcm