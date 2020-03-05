#include <http/http_router.h>
#include <utils/log.h>
#include "keep_alive_controller.h"
#include "dispatcher/dispatch_manager.h"
#include "proto/dao/account.pb.h"
#include "fiber/fiber_pool.h"
#include <metrics_client.h>
#include "utils/time.h"
#include "group/group_msg_service.h"

using namespace bcm;
namespace http = boost::beast::http;
using namespace bcm::metrics;

static constexpr char kMetricsKeepAliveServiceName[] = "keepalive";

//#define BACKEND "183.36.111.207"
//#define BACKEND_PORT "33000"
//#define TARGET_PREFIX "/group/"

KeepAliveController::KeepAliveController(std::shared_ptr<DispatchManager> dispatchManager,
                                         std::shared_ptr<GroupMsgService> groupMsgService)
    : m_dispatchManager(std::move(dispatchManager))
    , m_groupMsgService(groupMsgService)
{
}

KeepAliveController::~KeepAliveController()
{
}

void KeepAliveController::addRoutes(bcm::HttpRouter& router)
{
    router.add(http::verb::get, "/v1/keepalive", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&KeepAliveController::getKeepAlive, shared_from_this(), std::placeholders::_1));
}

void KeepAliveController::getKeepAlive(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& response = context.response;
    if (!context.authResult.empty()) {
        auto* account = boost::any_cast<Account>(&context.authResult);

        LOGI << "keep alive uid: " << account->uid() << " deviceid:" << account->authdeviceid();

        bool isAccountAvailable = true;
        DispatchAddress address(account->uid(), account->authdeviceid());
        if (account->state() == Account::DELETED) {
            LOGW << "account is deleted, uid: " << account->uid() << " deviceid:" << account->authdeviceid();
            m_dispatchManager->kick(address);
            isAccountAvailable = false;
        }

        if (!m_dispatchManager->hasLocalSubscription(address)) {
            LOGW << "no local subscription found, uid: " << account->uid() << "deviceid:" << account->authdeviceid();
            m_dispatchManager->kick(address);
            isAccountAvailable = false;
        }

        if (isAccountAvailable) {
            m_groupMsgService->notifyUserOnline(address);
        }
    }

    response.result(http::status::ok);

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeepAliveServiceName,
        "getKeepAlive", (nowInMicro() - dwStartTime), 0);

}
