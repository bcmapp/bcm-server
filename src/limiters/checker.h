#pragma   once
#include <map>
#include "common/api.h"

namespace bcm {
    
namespace http = boost::beast::http;

struct CheckArgs {
    Api api;
    std::string uid;
    std::string origin;
};

enum CheckResult {
    PASSED = 0,
    FAILED,
    ABORT
};

class IChecker {
public:
public:
    virtual CheckResult check(CheckArgs* args) = 0;
};

}