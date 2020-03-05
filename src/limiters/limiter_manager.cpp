#include "limiter_manager.h"
#include "configuration_manager.h"

namespace bcm {

LimiterManager::LimiterManager() : m_limiters()
{

}

std::pair<std::shared_ptr<ILimiter>, bool> LimiterManager::emplace(const std::string& id, const std::shared_ptr<ILimiter>& limiter)
{
    boost::unique_lock<boost::shared_mutex> guard(m_mutex);
    auto res = m_limiters.emplace(id, limiter);
    return std::make_pair(res.first->second, res.second);
}

std::shared_ptr<ILimiter> LimiterManager::find(const std::string& id)
{
    boost::shared_lock<boost::shared_mutex> guard(m_mutex);
    auto it = m_limiters.find(id);
    if (it == m_limiters.end()) {
        return nullptr;
    }
    return it->second;
}

void LimiterManager::erase(const std::string &id)
{
    boost::unique_lock<boost::shared_mutex> guard(m_mutex);
    m_limiters.erase(id);
}

LimiterManager* LimiterManager::getInstance() 
{
    static LimiterManager instance;
    return &instance; 
}

}