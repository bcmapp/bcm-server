#pragma  once
#include <set>
#include <thread>
#include <mutex>
#include <event2/event.h>
#include <boost/thread/shared_mutex.hpp>
#include "common/observer.h"
#include "dao/client.h"
#include "redis/async_conn.h"

namespace bcm {

struct LimiterConfigUpdateEvent {
    dao::LimiterConfigurations::LimiterConfigs configs;
    std::set<std::string> removed;
};

/**
 * Single instance manager to manage all of the rate limiters' configuration. Registering your 
 * rate limiter into this manager if you want the ability to automaticlly update the local 
 * configuration while the value on remote server has been changed.
 */
class LimiterConfigurationManager : public Observable<LimiterConfigUpdateEvent> {
private:
    typedef std::set<std::shared_ptr<ObserverType>> ObserverSet;
public:
    virtual ~LimiterConfigurationManager() {}
    
    virtual void notify(const LimiterConfigUpdateEvent& event) override;

    virtual void registerObserver(const std::shared_ptr<ObserverType>& observer) override;

    virtual void unregisterObserver(const std::shared_ptr<ObserverType>& observer) override;

    void initialize();

    void uninitialize();

    void reloadConfiguration();

    void getConfiguration(const std::set<std::string>& keys, 
                          dao::LimiterConfigurations::LimiterConfigs& configs);

    static LimiterConfigurationManager* getInstance();

private:
    LimiterConfigurationManager();

#ifdef UNIT_TEST
public:
#endif
    ObserverSet m_observers;
    std::shared_ptr<dao::LimiterConfigurations> m_configDao;
    dao::LimiterConfigurations::LimiterConfigs m_configs;
    boost::shared_mutex m_mutex;
};

}