#pragma     once
#include <list>
#include "checker.h"
#include "limiter.h"
#include "api_matcher.h"
#include "common/observer.h"

namespace bcm {

class ApiChecker : public IChecker {
public:
    ApiChecker(std::shared_ptr<IApiMatcher>& apiMatcher);
    virtual CheckResult check(CheckArgs* args) override;
    bool ignore(CheckArgs* args);

private:
    void getApiIdentity(const CheckArgs* args, std::string& id);

private:
    std::shared_ptr<IApiMatcher> m_apiMatcher;
    std::list<std::shared_ptr<ILimiter>> m_limiters;
};

}