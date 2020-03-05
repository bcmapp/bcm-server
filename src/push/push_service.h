#pragma once

#include <vector>
#include <boost/asio.hpp>
#include "config/apns_config.h"
#include "config/fcm_config.h"
#include "config/umeng_config.h"
#include "../config/group_store_format.h"
#include "controllers/system_entities.h"
#include "push_notification.h"

namespace bcm {
class Account;
class Device;
class AccountsManager;
class RedisConfig;
} // namespace bcm

namespace bcm {
namespace push {
class ServiceImpl;
// -----------------------------------------------------------------------------
// Section: Service
// -----------------------------------------------------------------------------
class Service : public std::enable_shared_from_this<Service> {
    typedef boost::asio::io_context io_context;
    typedef boost::system::error_code error_code;

public:
    typedef std::shared_ptr<Service> shared_ptr;

    Service(std::shared_ptr<AccountsManager> accountMgr, 
            const RedisConfig& redisCfg, const ApnsConfig& apns, 
            const FcmConfig& fcm, const UmengConfig& umeng, 
            int concurrency = 5);
    ~Service();

    void sendNotification(const std::string& pushType, const Notification& noti);
    void broadcastNotification(const std::string& topic, const Notification& noti);

    shared_ptr exponentialBackoff(int32_t initialDelayMillis,
                                  double multiplier);
    shared_ptr uniformJitter();
    shared_ptr maxDelay(int32_t maxDelayMillis);
    shared_ptr maxRetries(int32_t times);

private:
    ServiceImpl* m_pImpl;
    ServiceImpl& m_impl;
};

} // namespace push
} // namespace bcm

namespace bcm {
typedef push::Service PushService;
} // namespace bcm
