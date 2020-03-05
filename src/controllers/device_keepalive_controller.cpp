#include <http/http_router.h>
#include <utils/log.h>
#include "device_keepalive_controller.h"
#include "dispatcher/dispatch_manager.h"
#include "proto/dao/account.pb.h"
#include "fiber/fiber_pool.h"
#include <metrics_client.h>
#include "utils/time.h"
#include "group/group_msg_service.h"

using namespace bcm;
namespace http = boost::beast::http;
using namespace bcm::metrics;

static constexpr char kMetricsKeepAliveServiceName[] = "devicekeepalive";


DeviceKeepaliveController::DeviceKeepaliveController(std::shared_ptr<DispatchManager> dispatchManager)
    : m_dispatchManager(std::move(dispatchManager))
{
}

DeviceKeepaliveController::~DeviceKeepaliveController()
{
}

void DeviceKeepaliveController::addRoutes(bcm::HttpRouter& router)
{
    router.add(http::verb::get, "/v1/keepalive/device", Authenticator::AUTHTYPE_NO_AUTH,
               std::bind(&DeviceKeepaliveController::deviceKeepAlive, shared_from_this(), std::placeholders::_1));
}

void DeviceKeepaliveController::deviceKeepAlive(bcm::HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeepAliveServiceName,
        "deviceKeepAlive", (nowInMicro() - dwStartTime), 0);
    return context.response.result(http::status::ok);
}

