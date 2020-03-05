#pragma    once
#include <list>
#include <memory>
#include "checker.h"
#include "limiter.h"
#include "api_matcher.h"

namespace bcm {

class UidChecker : public IChecker {
public:
    UidChecker(std::shared_ptr<IApiMatcher>& apiMatcher);
    virtual CheckResult check(CheckArgs* args) override;
    bool ignore(const CheckArgs* args);

    void preProcessing(CheckArgs* args);

private:
    std::shared_ptr<IApiMatcher> m_apiMatcher;
    std::list<std::shared_ptr<ILimiter>> m_limiters;
};

}