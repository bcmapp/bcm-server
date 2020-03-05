#pragma once

#include <dao/client.h>

namespace bcm {

class LimitersManager {
public:
    int getLimiters(const std::set<std::string>& keys, std::map<std::string, Limiter>& limiters);

    bool setLimiters(const std::map<std::string, Limiter>& limiters);

    int getLimiter(const std::string& key, Limiter& limiter);

    bool setLimiter(const std::string& key, Limiter& limiter);

private:
    std::shared_ptr<dao::Limiters> m_limiters{dao::ClientFactory::limiters()};
};

}
