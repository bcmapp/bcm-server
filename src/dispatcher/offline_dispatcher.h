#pragma once

#include <string>
#include <registers/offline_register.h>
#include <push/push_notification.h>

namespace bcm {

constexpr char kOfflineNotificationsUrl[] = "/v1/offline/notifications";

class OfflineDispatcher {
public:
    explicit OfflineDispatcher(std::shared_ptr<OfflineServiceRegister> offlineServiceRegister);
    ~OfflineDispatcher() = default;

    void dispatch(const std::string& pushType, const push::Notification& notification);
    void dispatch(const std::string& pushType, const push::Notifications& notifications);

private:
    std::shared_ptr<OfflineServiceRegister> m_offlineServiceRegister;

};

}

