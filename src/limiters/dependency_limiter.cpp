#include "dependency_limiter.h"
#include "utils/log.h"

namespace bcm {

DependencyLimiter::DependencyLimiter(const std::shared_ptr<ILimiter>& limiter, 
                                     const std::vector<std::shared_ptr<ILimiter>>& dependencies)
    : m_limiter(limiter),
      m_dependencies(dependencies)
{

}

LimitLevel DependencyLimiter::acquireAccess(const std::string& id) {
    for (const auto& d : m_dependencies) {
        if (LimitLevel::LIMITED == d->limited(id)) {
            LOGE << "dependency limiter " << d->identity() << " is in limited state for id: " << id;
            return LimitLevel::LIMITED;
        }
    }

    return m_limiter->acquireAccess(id);
}

LimitLevel DependencyLimiter::limited(const std::string& id) {
    return m_limiter->limited(id);
}

void DependencyLimiter::currentState(LimitState& state) {
    m_limiter->currentState(state);
}

}
