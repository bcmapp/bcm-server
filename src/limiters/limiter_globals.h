#pragma once
#include <set>
#include <functional>
#include "common/api.h"
#include "common/observer.h"

namespace bcm {

class LimiterGlobals : public Observer<Api> {
public:
    // use greater here because we want the api pattern like
    // ':uid' to be tested at last, other wise there would be
    // mistakes while matching patterns like "/v1/profile/:uid" 
    // and "/v1/profile/key" 
    typedef std::set<Api, std::greater<Api>> ApiSet;

public:
    static std::shared_ptr<LimiterGlobals> getInstance();

    static const ApiSet& getActiveApis() { return kActiveApis; }

    const ApiSet& getIgnoredApis() { return m_ignoredApis; }

    void update(const Api& api) override;
#if 0
    void dump();
#endif

public:
    static std::string kLimiterServiceName;

private:
    LimiterGlobals() {}

#ifdef UNIT_TEST
public:
#endif
    static ApiSet kActiveApis;
    ApiSet m_ignoredApis;
};

}

