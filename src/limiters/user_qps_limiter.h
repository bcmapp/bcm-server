#pragma   once
#include <boost/thread/shared_mutex.hpp>
#include "limiter.h"
#include "configuration_manager.h"

namespace bcm {

const static std::string kUserQpsRuleKey = "user/qps";
class UserQpsLimiter : public ILimiter, 
                       public Observer<LimiterConfigUpdateEvent> {
public:
    UserQpsLimiter() : m_rule(1000, 1000) 
    { 
    }

    virtual LimitLevel acquireAccess(const std::string& id) override;

    virtual void currentState(LimitState& state) override;

    virtual LimitLevel limited(const std::string& id) override;

    virtual void update(const LimiterConfigUpdateEvent& event) override;

    virtual std::string identity() override { return kIdentity; }

#ifndef UNIT_TEST
private:
#endif
    struct Item {
        Item() : mutex(new boost::shared_mutex()) {}
        Status status;
        std::shared_ptr<boost::shared_mutex> mutex;
    };
    dao::LimitRule m_rule;
    std::map<std::string, Item> m_status;
    boost::shared_mutex m_statusMutex;
    boost::shared_mutex m_ruleMutex;

public:
    static const std::string kIdentity;
};

}