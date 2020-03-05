#pragma once

#include "apns_notification.h"

#include <boost/system/error_code.hpp>

#include <string>

namespace bcm {
class ApnsConfig;
class RedisConfig;
class DispatchAddress;
};

namespace bcm {
namespace push {
namespace apns {

class Service;
class QosMgrImpl;

class QosMgr {
public:
    struct INotificationSender {
        virtual ~INotificationSender() { }
        virtual void sendNotification(const std::string& apnsType, 
                                      const Notification& notification) = 0;
    };

    QosMgr(const ApnsConfig& apnsCfg, const RedisConfig& redisCfg, 
           INotificationSender& sender);
    ~QosMgr();

    void scheduleResend(const DispatchAddress& addr, 
                        const std::string& apnsType, 
                        std::unique_ptr<Notification> notification);
private:
    QosMgrImpl* m_pImpl;
    QosMgrImpl& m_impl;
};

} // namespace apns
} // namespace push
} // namespace bcm