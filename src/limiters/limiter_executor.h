#pragma once
#include <memory>
#include <list>
#include "checker.h"
#include "api_matcher.h"
#include "http/http_validator.h"

namespace bcm {

class LimiterExecutor : public IValidator {
public:
    LimiterExecutor();
    virtual ~LimiterExecutor();

    virtual bool validate(const ValidateInfo& info, uint32_t& status) override;

#ifndef UNIT_TEST
protected:
#endif
    bool ignore(const Api& api);
private:
    std::shared_ptr<IApiMatcher> m_apiMatcher;
    std::list<std::shared_ptr<IChecker>> m_checkers;
};

}
