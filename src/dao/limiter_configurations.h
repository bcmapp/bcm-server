#pragma  once
#include <string>
#include <map>
#include "proto/dao/error_code.pb.h"

namespace bcm {
namespace dao {

struct LimitRule {
    LimitRule() : period(0), count(0) {}
    LimitRule (int64_t p, int64_t c) : period(p), count(c) {}
    // in milliseconds
    int64_t period;
    int64_t count;
};

class LimiterConfigurations 
{
public:
    typedef std::map<std::string, LimitRule> LimiterConfigs;
public:
    virtual ErrorCode load(LimiterConfigs& configs) = 0;

    virtual ErrorCode get(const std::set<std::string>& keys, LimiterConfigs& configs) = 0;

    virtual ErrorCode set(const LimiterConfigs& configs) = 0;
};

}
}
