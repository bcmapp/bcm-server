#pragma once
#include <boost/thread/shared_mutex.hpp>
#include "limiter.h"
#include "configuration_manager.h"
#include "api_matcher.h"

namespace bcm {

const static std::string kDefaultApiQpsRuleKey = "default";
const static std::string kApiQpsRuleKeyPrefix = "api/qps/";


class ApiQpsLimiter : public ILimiter, 
                      public Observer<LimiterConfigUpdateEvent> {

public:
    ApiQpsLimiter(std::shared_ptr<IApiMatcher>& apiMatcher);

    virtual LimitLevel acquireAccess(const std::string& id) override;

    virtual void currentState(LimitState& state) override;

    virtual LimitLevel limited(const std::string& id) override;

    virtual void update(const LimiterConfigUpdateEvent& event) override;

    virtual std::string identity() override { return kIdentity; }

public:
    static const std::string kIdentity;

#ifndef UNIT_TEST
private:
#endif
    struct Item {
        Item() : mutex(new boost::shared_mutex()) {}
        Status status;
        std::shared_ptr<boost::shared_mutex> mutex;
    };
    std::shared_ptr<IApiMatcher> m_apiMatcher;
    std::map<std::string, dao::LimitRule> m_rules;
    std::map<std::string, Item> m_status;
    std::set<std::string, std::greater<std::string>> m_apiPatterns;
    boost::shared_mutex m_statusMutex;
    boost::shared_mutex m_ruleMutex;
};

}