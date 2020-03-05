#include "offline_dispatcher.h"
#include <config/group_store_format.h>
#include <http/http_client.h>

namespace bcm {

OfflineDispatcher::OfflineDispatcher(std::shared_ptr<bcm::OfflineServiceRegister> offlineServiceRegister)
    : m_offlineServiceRegister(std::move(offlineServiceRegister))
{
}

void OfflineDispatcher::dispatch(const std::string& pushType, const push::Notification& notification)
{
    push::Notifications notifications;
    notifications.content.push_back(notification);
    dispatch(pushType, notifications);
}

void OfflineDispatcher::dispatch(const std::string& pushType, const push::Notifications& notifications)
{
    std::string dispatchAddress = m_offlineServiceRegister->getRandomOfflineServerByType(pushType);

    if (dispatchAddress.empty()) {
        LOGW << "no available offline push server for: " << pushType;
        return;
    }

    HttpPut put("https://" + dispatchAddress + kOfflineNotificationsUrl);
    put.body("application/json", jsonable::toPrintable(notifications));

    if (!put.process(*FiberPool::getThreadIOContext())) {
        LOGW << "send notification request failed!";
        return;
    }

    if (put.response().result() != http::status::ok) {
        LOGW << "send notification request failed: " << put.response().result();
    }

    LOGD << "dispatch notification: " << jsonable::toPrintable(notifications)
         << ", to: " << dispatchAddress << ", type: " << pushType;
}

}