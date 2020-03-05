#pragma once
#include "limiter.h"
#include <vector>

#ifdef UNIT_TEST
#define private public
#define protected public
#endif

namespace bcm {

class DependencyLimiter : public ILimiter {
public:
    

public:
    DependencyLimiter(const std::shared_ptr<ILimiter>& limiter, 
                      const std::vector<std::shared_ptr<ILimiter>>& dependencies);

    virtual LimitLevel acquireAccess(const std::string& id) override;

    virtual void currentState(LimitState& state) override;

    virtual LimitLevel limited(const std::string& id) override;

    virtual std::string identity() override 
    {
        if (m_limiter) { 
            return m_limiter->identity();
        }
        return "";
    }

private:
    std::shared_ptr<ILimiter> m_limiter;
    std::vector<std::shared_ptr<ILimiter>> m_dependencies;
};

}

#ifdef UNIT_TEST
#undef private
#undef protected
#endif
