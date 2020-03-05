#include "offline_server_controller.h"

#include "../../config/group_store_format.h"

#include "utils/log.h"
#include "utils/time.h"

#include "offline_server_entities.h"

#include "push/push_service.h"

#include "redis/hiredis_client.h"

#include "dispatcher/idispatcher.h"
#include "dispatcher/dispatch_manager.h"

#include "proto/dao/account.pb.h"
#include "store/accounts_manager.h"

#include "fiber/fiber_pool.h"

#include "crypto/base64.h"


using namespace bcm;
namespace http = boost::beast::http;

static constexpr char kMetricsGrpControllerServiceName[] = "offline_controller";

OfflinePushController::OfflinePushController(std::shared_ptr<PushService> pushService,
                                             OfflineServerConfig config)
    : m_pushService(std::move(pushService))
    , m_config(std::move(config))
{
}

OfflinePushController::~OfflinePushController() = default;

void OfflinePushController::addRoutes(bcm::HttpRouter& router)
{
    router.add(http::verb::post, kOfflinePushMessageUrl, Authenticator::AUTHTYPE_NO_AUTH,
               std::bind(&OfflinePushController::sendGroupOfflinePush, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<GroupOfflinePushMessage>);

    router.add(http::verb::put, kOfflineNotificationsUrl, Authenticator::AUTHTYPE_NO_AUTH,
               std::bind(&OfflinePushController::dispatchNotifications, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<push::Notifications>);
}

void OfflinePushController::sendGroupOfflinePush(HttpContext& context)
{
    LOGI << "POST /v1/offline/pushmsg " << context.request.body();
    auto* msg = boost::any_cast<GroupOfflinePushMessage>(&context.requestEntity);
    
    for (const auto& itDesc : msg->destinations) {
        GroupUserMessageIdInfo gumi;
        gumi.from_string(itDesc.second);

        push::Notification notification;
        notification.group(msg->groupId, msg->messageId);
        notification.setApnsType(gumi.apnType);
        notification.setApnsId(gumi.apnId);
        notification.setVoipApnsId(gumi.voipApnId);
        notification.setFcmId(gumi.gcmId);
        notification.setUmengId(gumi.umengId);

        ClientVersion cv;
        cv.set_ostype(static_cast<ClientVersion::OSType>(gumi.osType));
        cv.set_bcmbuildcode(gumi.bcmBuildCode);
        cv.set_phonemodel(gumi.phoneModel);
        cv.set_osversion(gumi.osVersion);
        notification.setClientVersion(std::move(cv));

        auto address = DispatchAddress::deserialize(gumi.targetAddress);
        if (address != boost::none) {
            notification.setTargetAddress(*address);
        }
        std::string pushType = notification.getPushType();
        if (!m_config.checkPushType(pushType)) {
            LOGW << "unsupport push type: " << pushType;
            context.response.result(http::status::bad_request);
            return;
        }

        if (m_config.isPush) {
            m_pushService->sendNotification(notification.getPushType(), notification);
        }
    }
    
    context.response.result(http::status::ok);
}

void OfflinePushController::dispatchNotifications(HttpContext& context)
{
    auto& notifications = boost::any_cast<push::Notifications&>(context.requestEntity);
    context.response.result(http::status::ok);

    if (!m_config.isPush) {
        return;
    }

    for (auto& notification : notifications.content) {
        std::string pushType = notification.getPushType();
        if (!m_config.checkPushType(pushType)) {
            LOGW << "unsupport push type: " << pushType << "-" << jsonable::toPrintable(notification);
            continue;
        }

        m_pushService->sendNotification(notification.getPushType(), notification);
    }
}
