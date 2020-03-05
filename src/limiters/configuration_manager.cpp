#include <future>
#include "configuration_manager.h"
#include "utils/log.h"

namespace bcm {

LimiterConfigurationManager::LimiterConfigurationManager() 
    : m_observers(), m_configDao(nullptr), m_configs() {}

void LimiterConfigurationManager::notify(const LimiterConfigUpdateEvent& event)
{
    for (auto& item : m_observers) {
        if (item == nullptr) {
            continue;
        }
        item->update(event);
    }
}

void LimiterConfigurationManager::registerObserver(const std::shared_ptr<ObserverType>& observer) 
{
    m_observers.emplace(observer);
}

void LimiterConfigurationManager::unregisterObserver(const std::shared_ptr<ObserverType>& observer) 
{
    m_observers.erase(observer);
}

void LimiterConfigurationManager::initialize() 
{
    if (m_configDao == nullptr) {
        m_configDao = dao::ClientFactory::limiterConfigurations();
    }
    //reloadConfiguration();
}

void LimiterConfigurationManager::uninitialize()
{
    if (m_configDao != nullptr) {
        m_configDao.reset();
    }
}

void LimiterConfigurationManager::reloadConfiguration() 
{
    if (m_configDao == nullptr) {
        LOGE << "failed to initialize limiterConfigurations";
        return;
    }
    dao::LimiterConfigurations::LimiterConfigs configs;
    auto rc = m_configDao->load(configs);
    if (rc != dao::ErrorCode::ERRORCODE_SUCCESS && rc != dao::ErrorCode::ERRORCODE_NO_SUCH_DATA) {
        LOGE << "failed to load limiter configuration, error: " << rc;
        return;
    }
    if (rc == dao::ErrorCode::ERRORCODE_SUCCESS) {
        dao::LimiterConfigurations::LimiterConfigs copy;
        {
            boost::unique_lock<boost::shared_mutex> guard(m_mutex);
            m_configs.swap(configs);
            copy = m_configs;
        }
        LimiterConfigUpdateEvent event;
        event.configs = m_configs;
        for (const auto& it : configs) {
            if (copy.find(it.first) == copy.end()) {
                event.removed.emplace(it.first);
            }
        }
        notify(event);
    }
}

void LimiterConfigurationManager::getConfiguration(const std::set<std::string>& keys,
                                                   dao::LimiterConfigurations::LimiterConfigs& configs) 
{
    configs.clear();
    dao::LimiterConfigurations::LimiterConfigs copy;
    {
        boost::shared_lock<boost::shared_mutex> guard(m_mutex);
        copy = m_configs;
    }
    for (const auto& k : keys) {
        auto it = copy.find(k);
        if (it != copy.end()) {
            configs.emplace(*it);
        }
    }
}

LimiterConfigurationManager* LimiterConfigurationManager::getInstance()
{
    static LimiterConfigurationManager instance;
    return &instance;
}

}
