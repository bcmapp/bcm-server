#pragma  once
#include "limiter.h"
#include <boost/thread/shared_mutex.hpp>

namespace bcm {

class LimiterManager {
public:
    typedef std::map<std::string, std::shared_ptr<ILimiter>> LImiterMap;

public:
    std::pair<std::shared_ptr<ILimiter>, bool> emplace(const std::string& id, const std::shared_ptr<ILimiter>& limiter);
    std::shared_ptr<ILimiter> find(const std::string& id);

    void erase(const std::string& id);

    static LimiterManager* getInstance();

private:
    LimiterManager();

private:
    LImiterMap m_limiters;
    boost::shared_mutex m_mutex;
};

}