#include "limiters_manager.h"
#include <utils/log.h>

namespace bcm {

int LimitersManager::getLimiters(const std::set<std::string>& keys, std::map<std::string, Limiter>& limiters)
{
    auto error = m_limiters->getLimiters(keys, limiters);

    if (error == dao::ERRORCODE_NO_SUCH_DATA) {
        LOGT << "not any limiter found";
        return 0;
    }

    if (error != dao::ERRORCODE_SUCCESS) {
        LOGE << "get limiters failed: " << error;
        return -1;
    }

    return 1;
}

bool LimitersManager::setLimiters(const std::map<std::string, Limiter>& limiters)
{
    auto error = m_limiters->setLimiters(limiters);

    if (error != dao::ERRORCODE_SUCCESS) {
        LOGE << "store limiters failed: " << error;
        return false;
    }

    return true;
}

int LimitersManager::getLimiter(const std::string& key, Limiter& limiter)
{
    std::set<std::string> keys;
    std::map<std::string, Limiter> limiters;

    keys.insert(key);
    auto ret = getLimiters(keys, limiters);
    if (ret > 0) {
        limiter = limiters[key];
    }

    return ret;
}

bool LimitersManager::setLimiter(const std::string& key, Limiter& limiter)
{
    std::map<std::string, Limiter> limiters;

    limiters[key] = limiter;
    return setLimiters(limiters);
}

}