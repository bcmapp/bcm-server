#include "system_controller.h"
#include "system_entities.h"
#include "dao/accounts.h"
#include <http/http_router.h>
#include <utils/log.h>
#include "utils/time.h"
#include "fiber/fiber_pool.h"
#include <metrics_client.h>

namespace bcm {

using namespace metrics;

static constexpr char kMetricsSystemServiceName[] = "system";
static constexpr char kSystemTopic[] = "system_broadcast";

InnerSystemController::InnerSystemController(std::shared_ptr<AccountsManager> ptrAccountsManager,
                                             std::shared_ptr<PushService> pushService,
                                             std::shared_ptr<dao::SysMsgs> sysMsgs,
                                             const SysMsgConfig& conf)
    : m_ptrAccountsManager(std::move(ptrAccountsManager))
    , m_pushService(std::move(pushService))
    , m_sysMsgs(std::move(sysMsgs))
    , m_openService(conf.openSysMsgService)
{
}

void InnerSystemController::addRoutes(bcm::HttpRouter& router)
{
    router.add(http::verb::post, "/v1/system/push_system_message", Authenticator::AUTHTYPE_NO_AUTH,
               std::bind(&InnerSystemController::sendSystemOfflinePush, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<SysMsgRequest>, nullptr);
}

void InnerSystemController::sendSystemOfflinePush(HttpContext& context)
{
    if (!m_openService) {
        return context.response.result(http::status::not_found);
    }

    int64_t dwStartTime = nowInMicro();
    auto& response = context.response;
    response.result(http::status::ok);

    auto* entity = boost::any_cast<SysMsgRequest>(&context.requestEntity);
    push::Notification notification;
    notification.system(entity->msg);

    if (entity->type == BROADCAST) {
        bcm::SysMsg msg;
        msg.set_destination("all");
        msg.set_sysmsgid(entity->msg.id);
        msg.set_content(nlohmann::json(entity->msg).dump());
        auto ret = m_sysMsgs->insert(msg);
        if (ret != dao::ERRORCODE_SUCCESS) {
            LOGW << "insert sys msg for all failed: " << entity->msg.id;
            response.result(http::status::internal_server_error);
        } else {
            m_pushService->broadcastNotification(kSystemTopic, notification);
        }
    } else if (entity->type == UNICAST) {
        std::vector<Account> vecAccount;
        std::vector<std::string> missedUids;
        m_ptrAccountsManager->get(entity->destination_uids, vecAccount, missedUids);
        if (missedUids.size() > 0) {
            std::string lost;
            for (auto uid : missedUids) {
                lost = lost + uid + ",";
            }
            LOGI << "there is not account for uids (" << lost << ").";
        }

        for (const auto& account : vecAccount) {
            auto device = AccountsManager::getDevice(account, Device::MASTER_ID);
            if (!device || !AccountsManager::isDevicePushable(*device)) {
                continue;
            }
            bcm::SysMsg msg;
            msg.set_destination(account.uid());
            msg.set_sysmsgid(entity->msg.id);
            msg.set_content(nlohmann::json(entity->msg).dump());
            auto ret = m_sysMsgs->insert(msg);
            if (ret != dao::ERRORCODE_SUCCESS) {
                LOGW << "insert sys msg for: " << account.uid() << " :failed: " << entity->msg.id;
            }

            notification.setTargetAddress(DispatchAddress(account.uid(), device->id()));
            notification.setDeviceInfo(device.get());

            std::string pushType = notification.getPushType();
            if (pushType.empty()) {
                continue;
            }
            // TODO: check and send directly
            m_pushService->sendNotification(pushType, notification);
        }

    } else {
        LOGE << "invalid type of push: "
             << entity->type << ": " << entity->msg.id;
        response.result(http::status::bad_request);
    }
    LOGI << "system msg push ends ";

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsSystemServiceName,
        "sendSystemOfflinePush", (nowInMicro() - dwStartTime), 0);

}


SystemController::SystemController(std::shared_ptr<bcm::dao::SysMsgs> sysMsgs)
    : m_sysMsgs(std::move(sysMsgs))
{
}

void SystemController::addRoutes(bcm::HttpRouter& router)
{
    router.add(http::verb::get, "/v1/system/msgs", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&SystemController::getSystemMsgs, shared_from_this(), std::placeholders::_1),
               nullptr, new JsonSerializerImp<SysMsgResponse>);
    router.add(http::verb::delete_, "/v1/system/msgs/:mid", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&SystemController::ackSystemMsgs, shared_from_this(), std::placeholders::_1),
               nullptr, nullptr);
}

void SystemController::getSystemMsgs(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& statics = context.statics;
    statics.setUid(account->uid());
    SysMsgResponse msgResponse;
    context.responseEntity = msgResponse;

    auto internalError = [&](const std::string& uid, const std::string& op) {
        LOGW << "internal error, uid: " << uid << " op: " << op;
        return res.result(http::status::internal_server_error);
    };

    std::vector<bcm::SysMsg> msgs;

    auto ret = m_sysMsgs->get(account->uid(), msgs);

    if (ret != dao::ERRORCODE_SUCCESS) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsSystemServiceName,
            "getSystemMsgs", (nowInMicro() - dwStartTime), 1003);
        return internalError(account->uid(), "get sys msgs");
    }

    std::vector<bcm::SysMsg> msgsAll;
    ret = m_sysMsgs->get("all", msgsAll);
    if (ret != dao::ERRORCODE_SUCCESS && ret != dao::ERRORCODE_NO_SUCH_DATA) {
        LOGW << "get sys msg for all failed: " << account->uid() << ": " << ret;
    } else {
        msgs.insert(msgs.end(), msgsAll.begin(), msgsAll.end());
    }

    LOGD << "get sys msg size for uid: " << account->uid() << " :" << msgs.size();

    std::vector<bcm::SysMsgContent> toSendMsgs;
    for (const auto& msg : msgs) {
        try {
            SysMsgContent sysMsgContent = nlohmann::json::parse(msg.content());
            auto content = nlohmann::json::parse(sysMsgContent.content);
            if (content.find("endtime") != content.end() && content["endtime"].is_number_unsigned()) {
                int64_t endtime;
                jsonable::toNumber(content, "endtime", endtime);
                if (endtime <= nowInSec()) {
                    LOGW << "sys msg expires: " << nlohmann::json(sysMsgContent);
                    m_sysMsgs->del(msg.destination(), msg.sysmsgid());
                } else {
                    toSendMsgs.push_back(sysMsgContent);
                }
            }
        } catch (const std::exception& e) {
            LOGW << "sys msg content bad: " << msg.Utf8DebugString();
            m_sysMsgs->del(msg.destination(), msg.sysmsgid());
        }
    }
    std::sort(toSendMsgs.begin(), toSendMsgs.end(), [](const SysMsgContent& a, const SysMsgContent& b){
            bool result = false;
            if (a.activityId != b.activityId) {
                result = a.activityId < b.activityId;
            } else {
                result = a.id >= b.id;
            }
            return result;
        });
    auto uniqueIter = std::unique(toSendMsgs.begin(),toSendMsgs.end(),
            [](const SysMsgContent& a, const SysMsgContent& b){
                return a.activityId == b.activityId;
            });
    toSendMsgs.erase(uniqueIter, toSendMsgs.end());
    std::sort(toSendMsgs.begin(), toSendMsgs.end(), [](const SysMsgContent& a, const SysMsgContent& b){return a.id <= b.id;});

    msgResponse.msgs = toSendMsgs;
    LOGD << "sys msgs respone: " << account->uid() << ": " << nlohmann::json(msgResponse);
    context.responseEntity = msgResponse;
    res.result(http::status::ok);
    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsSystemServiceName,
            "getSystemMsgs", (nowInMicro() - dwStartTime), 0);
}

void SystemController::ackSystemMsgs(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    res.result(http::status::ok);
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& statics = context.statics;
    statics.setUid(account->uid());
    uint64_t mid = std::stol(context.pathParams[":mid"]);

    auto internalError = [&](const std::string& uid, const std::string& op) {
        LOGW << "internal error, uid: " << uid << " op: " << op;
        return res.result(http::status::internal_server_error);
    };

    auto ret = m_sysMsgs->delBatch(account->uid(), mid);

    if (ret != dao::ERRORCODE_SUCCESS) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsSystemServiceName,
            "ackSystemMsgs", (nowInMicro() - dwStartTime), 1003);
        return internalError(account->uid(), "del sys msgs");
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsSystemServiceName,
        "ackSystemMsgs", (nowInMicro() - dwStartTime), 0);
}

}
