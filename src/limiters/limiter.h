#pragma  once
#include <string>
#include <map>
#include <set>
#include "common/observer.h"

namespace bcm {

enum LimitLevel {
    GOOD = 0,
    LIMITED,
};

struct LimitState {
    std::string id;
    std::set<std::string> keys;
    std::map<std::string, int64_t> counters;
};

class ILimiter {
public:
    virtual LimitLevel acquireAccess(const std::string& id) = 0;
    virtual void currentState(LimitState& state) = 0;
    virtual LimitLevel limited(const std::string& id) = 0;

    virtual std::string identity() = 0;

protected:
    struct Status {
        Status() : startTime(0), counter(0) {}
        int64_t startTime;
        int64_t counter;
    };
};

}
