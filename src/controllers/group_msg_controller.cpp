#include "group_msg_controller.h"
#include "group_msg_entities.h"

#include "proto/dao/group_msg.pb.h"
#include "proto/dao/account.pb.h"
#include "proto/dao/client_version.pb.h"
#include "dao/client.h"

#include <metrics_client.h>

#include "group/group_msg_service.h"
#include "utils/time.h"
#include "utils/log.h"
#include "utils/account_helper.h"
#include "utils/sender_utils.h"
#include <crypto/base64.h>
#include "redis/redis_manager.h"
#include "redis/online_redis_manager.h"
#include "redis/reply.h"
#include "config/group_store_format.h"
#include "group/message_type.h"


namespace bcm {

using namespace metrics;

static const std::string groupMessageService = "bcm_gmessager";

static const std::string groupSendMsgTopic = "send_msg";
static const std::string groupRecallMsgTopic = "recall_msg";
static const std::string groupGetMsgTopic = "get_msg";
static const std::string groupMsgAckTopic = "ack_msg";
static const std::string groupPublishRedisTopic = "publish_redis";
static const std::string groupQueryMaxReadMidTopic = "query_last_mid";

static bool supportRecall(const ClientVersion& cliVer)
{
    if (cliVer.ostype() == ClientVersion::OSTYPE_IOS) {
        return (cliVer.bcmbuildcode() >= 527);
    } else if (cliVer.ostype() == ClientVersion::OSTYPE_ANDROID) {
        return true;
    } else {
        return true;
    }
}

std::ostream& operator<<(std::ostream& os, std::vector<std::string> stringlist)
{
    os << std::string("[");
    for (auto it = stringlist.begin(); it != stringlist.end(); ++it) {
        if (it != stringlist.begin()) {
            os << std::string(", ");
        }
        os << *it;
    }
    os << std::string("]");
    return os;
}

GroupMsgController::GroupMsgController(
    std::shared_ptr<GroupMsgService> groupMsgService,
    const EncryptSenderConfig& cfg,
    const SizeCheckConfig& scCfg)
    : m_groupMsgService(groupMsgService)
    , m_groups(dao::ClientFactory::groups())
    , m_groupUsers(dao::ClientFactory::groupUsers())
    , m_groupMsgs(dao::ClientFactory::groupMsgs())
    , m_encryptSenderConfig(cfg)
    , m_sizeCheckConfig(scCfg)
{
}

void GroupMsgController::addRoutes(HttpRouter& router)
{
    router.add(http::verb::put, "/v1/group/deliver/send_msg", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&GroupMsgController::sendMsg, shared_from_this(),
                         std::placeholders::_1),
               new JsonSerializerImp<GMsgRequest>,
               new JsonSerializerImp<GroupResponse<GMsgResult>>);

    router.add(http::verb::put, "/v1/group/deliver/recall_msg", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&GroupMsgController::recallMsg, shared_from_this(),
                         std::placeholders::_1),
               new JsonSerializerImp<GRecallRequest>,
               new JsonSerializerImp<GroupResponse<GMsgResult>>);

    router.add(http::verb::put, "/v1/group/deliver/get_msg", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&GroupMsgController::getMsg, shared_from_this(),
                         std::placeholders::_1),
                new JsonSerializerImp<GGetMsgRequest>,
                new JsonSerializerImp<GroupResponse<GGetMsgResult>>);

    router.add(http::verb::put, "/v1/group/deliver/ack_msg", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupMsgController::ackMsg, shared_from_this(),
                         std::placeholders::_1),
                new JsonSerializerImp<GAckMsgRequest>,
                new JsonSerializerImp<GroupResponse<GNullResult>>);

    router.add(http::verb::put, "/v1/group/deliver/query_last_mid", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&GroupMsgController::queryLastMid, shared_from_this(),
                         std::placeholders::_1),
               nullptr,
               new JsonSerializerImp<GroupResponse<GQueryLastMidResult>>);

    router.add(http::verb::post, "/v1/group/deliver/query_uids", Authenticator::AUTHTYPE_NO_AUTH,
               std::bind(&GroupMsgController::queryUids, shared_from_this(),
                         std::placeholders::_1),
               new JsonSerializerImp<GQueryUidsRequest>,
               new JsonSerializerImp<GroupResponse<GQueryUidsResult>>);
}

void GroupMsgController::sendMsg(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(groupMessageService, 
        groupSendMsgTopic);
    Account* account = boost::any_cast<Account>(&context.authResult);
    GMsgRequest* req = boost::any_cast<GMsgRequest>(&context.requestEntity);
    GroupResponse<GMsgResult> resp;

    std::stringstream ss;
    ss << req->atList;
    LOGT << "sendMsg request received, uid: " << account->uid()
         << ", gid: " << req->gid << ", text: " << req->text
         << ", atList: " << ss.str() << ", atAll: " << req->atAll;

    int64_t startTime = get_current_us();

    if (req->text.size() > m_sizeCheckConfig.messageSize) {
        LOGE << "message size " << req->text.size() << " is more than the limit " << m_sizeCheckConfig.messageSize;
        resp.errorCode = static_cast<int>(http::status::bad_request);
        resp.errorMsg = "message too long";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }

    Group groupInfo;
    dao::ErrorCode ec = m_groups->get(req->gid, groupInfo);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "get group info error: " << ec << ", gid: " << req->gid;
        resp.errorCode = 1;
        resp.errorMsg = "query database error";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }
    LOGI << "[spent] queryGroupInfo " 
         << (get_current_us() - startTime) << " us";
    startTime = get_current_us();

    GroupMsg::Type groupMsgType = GroupMsg::TYPE_UNKNOWN;
    if (groupInfo.broadcast() == 0 ) {
        groupMsgType = GroupMsg::TYPE_CHAT;
    } else if (groupInfo.broadcast() > 0) {
        groupMsgType = GroupMsg::TYPE_CHANNEL;
    } else {
        resp.errorCode = 100402;
        resp.errorMsg = "invalid broadcast value -1";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }

    GroupUser::Role role;
    ec = m_groupUsers->getMemberRole(req->gid, account->uid(), role);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "get member role error: " << ec << ", gid: " << req->gid 
             << ", uid: " << account->uid();
        resp.errorCode = 1;
        resp.errorMsg = "query database error";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }
    if (GroupUser::ROLE_UNDEFINE == role) {
        LOGE << "uid: " << account->uid() << " is not a member of group" 
             << ", gid: " << req->gid;
        resp.errorCode = 1;
        resp.errorMsg = "member not exist";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }
    if (GroupUser::ROLE_SUBSCRIBER == role) {
        LOGE << "member uid " << account->uid() 
             << " is a subscriber of group gid " << req->gid;
        resp.errorCode = 100401;
        resp.errorMsg = "subscriber cannot send group message";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }
    LOGI << "[spent] GroupUsers::getMemberRole " 
         << (get_current_us() - startTime) << " us";
    startTime = get_current_us();

    LOGI << "[spent] Groups::getMaxMid " 
         << (get_current_us() - startTime) << " us";
    startTime = get_current_us();

    nlohmann::json atList = req->atList;
    uint64_t createTime = get_current_ms();
    std::string sSourceExtra = "";
    GroupMsg groupMsg;
    bool valid = false;
    if (!req->pubKey.empty()) {
        valid = (getSourceExtra(account->uid(), req->pubKey, sSourceExtra) == 0);
        if (!valid) {
            LOGE << "encrypt sender failed, uid " << account->uid()
                 << " group message pubkey " << req->pubKey;
        }
    } else {
        valid = m_encryptSenderConfig.plainUidSupport;
        if (!valid) {
            LOGE << "unsupproted version of bcm app";
        }
    }
    if (!valid) {
        resp.errorCode = static_cast<int>(boost::beast::http::status::internal_server_error);
        resp.errorMsg = "internal server error";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }

    groupMsg.set_gid(req->gid);
    groupMsg.set_fromuid(m_encryptSenderConfig.plainUidSupport ? account->uid() : "");
    groupMsg.set_text(req->text);
    groupMsg.set_updatetime(createTime);
    groupMsg.set_type(groupMsgType);
    groupMsg.set_status(GroupMsg::STATUS_NORMAL);
    groupMsg.set_atall(req->atAll ? 1 : 0);
    groupMsg.set_atlist(atList.dump());
    groupMsg.set_createtime(createTime);
    groupMsg.set_sourceextra(sSourceExtra);
    groupMsg.set_verifysig(req->sig);
    uint64_t newMid = 0;
    ec = m_groupMsgs->insert(groupMsg, newMid);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "save group message error: " << ec << "uid: " << account->uid() 
             << ", gid: " << req->gid << ", text: " << req->text 
             << ", at list: " << atList.dump() << ", at all: " << req->atAll;
        resp.errorCode = 1;
        resp.errorMsg = "database error";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }

    LOGI << "[spent] GroupMsg::insert " 
         << (get_current_us() - startTime) << " us";
    startTime = get_current_us();

    groupMsg.set_mid(newMid);
    // filling fromuid with the account uid to avoid pushing to the sender himself during message push
    groupMsg.set_fromuid(account->uid());
    sendGroupChatMsg(groupMsg);

    GroupMultibroadMessageInfo groupMultibroadInfo;
    groupMultibroadInfo.from_uid = account->uid();
    m_groupMsgService->updateRedisdbOfflineInfo(req->gid, newMid, groupMultibroadInfo);

    resp.errorCode = 0;
    resp.errorMsg = "success";
    resp.result.gid = req->gid;
    resp.result.mid = newMid;
    resp.result.createTime = createTime;
    LOGI << "group message sent, " << "uid: " << account->uid() << ", gid: " 
         << req->gid << ", text: " << req->text << ", at list: " 
         << atList.dump() << ", at all: " << req->atAll
         << " ,newMid: " << newMid ;

    context.responseEntity = resp;
    LOGI << "[spent] done " << (get_current_us() - startTime) << " us";
}

void GroupMsgController::recallMsg(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(groupMessageService, 
        groupRecallMsgTopic);
    Account* account = boost::any_cast<Account>(&context.authResult);
    GRecallRequest* req = 
        boost::any_cast<GRecallRequest>(&context.requestEntity);
    GroupResponse<GMsgResult> resp;

    LOGT << "recallMsg request received, uid: " << account->uid()
         << ", gid: " << req->gid << ", mid: " << req->mid;

    GroupMsg groupMsg;
    dao::ErrorCode ec = m_groupMsgs->get(req->gid, req->mid, groupMsg);
    if (ec != dao::ERRORCODE_SUCCESS) {
        resp.errorCode = 1101004;
        resp.errorMsg = "query message error";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }

    bool senderVefied = false;
    if (!groupMsg.fromuid().empty()) {
        // if the fromuid is not empty, check whether the fromuid and account uid is equal
        if (groupMsg.fromuid() == account->uid()) {
            senderVefied = true;
        }
    } else {
        // otherwise, check the signature of random digit iv
        std::string iv = Base64::decode(req->iv);
        if (AccountHelper::verifySignature(account->publickey(), iv, groupMsg.verifysig())) {
            senderVefied = true;
        }
    }
    if (!senderVefied) {
        resp.errorCode = 1101002;
        resp.errorMsg = "cannot recall other's message";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }

    if (groupMsg.type() != GroupMsg::TYPE_CHAT && 
            groupMsg.type() != GroupMsg::TYPE_CHANNEL) {
        resp.errorCode = 1101002;
        resp.errorMsg = "should recall a chat message";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }

    if (groupMsg.status() == GroupMsg::STATUS_RECALLED) {
        resp.errorCode = 1101002;
        resp.errorMsg = "message is recalled";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }

    // recall a day before
    uint64_t now = get_current_ms();
    if ((now - groupMsg.createtime()) > (24 * 60 * 60 * 1000)) {
        resp.errorCode = 1101003;
        resp.errorMsg = "this message is weathered :-)";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }

    std::string sSourceExtra("");
    bool valid = false;
    if (!req->pubKey.empty()) {
        valid = (getSourceExtra(account->uid(), req->pubKey, sSourceExtra) == 0);
        if (!valid) {
            LOGE << "encrypt sender failed, uid " << account->uid()
                 << " group message pubkey " << req->pubKey;
        }
    } else {
        valid = m_encryptSenderConfig.plainUidSupport;
        if (!valid) {
            LOGE << "unsupproted version of bcm app";
        }
    }
    if (!valid) {
        resp.errorCode = static_cast<int>(boost::beast::http::status::internal_server_error);
        resp.errorMsg = "internal server error";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }
    uint64_t newMid;
    ec = m_groupMsgs->recall(sSourceExtra, m_encryptSenderConfig.plainUidSupport ? account->uid() : "", req->gid, req->mid, newMid);
    if (ec != dao::ERRORCODE_SUCCESS) {
        resp.errorCode = 1101004;
        resp.errorMsg = "recall message error";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }

    LOGT << "message recalled, uid: " << account->uid() 
         << ", gid: " << req->gid << ", mid: " << req->mid 
         << ", newMid: " << newMid;

    nlohmann::json textInDb = nlohmann::json::object({
        {"recalled_mid", req->mid}
    });

    GroupMsg msg;
    msg.set_gid(groupMsg.gid());
    // filling fromuid with the account uid to avoid pushing to the sender himself during message push
    msg.set_fromuid(account->uid());
    msg.set_text(textInDb.dump());
    msg.set_mid(newMid);
    msg.set_type(GroupMsg::TYPE_RECALL);
    msg.set_status(GroupMsg::STATUS_NORMAL);
    msg.set_atall(0);
    msg.set_atlist("[]");
    msg.set_createtime(now);
    msg.set_sourceextra(sSourceExtra);
    sendGroupChatMsg(msg);
    
    resp.errorCode = 0;
    resp.errorMsg = "success";
    resp.result.gid = req->gid;
    resp.result.mid = newMid;
    resp.result.createTime = now;
    context.responseEntity = resp;
}

void GroupMsgController::getMsg(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(groupMessageService, 
        groupGetMsgTopic);
    Account* account = boost::any_cast<Account>(&context.authResult);
    GGetMsgRequest* req = 
        boost::any_cast<GGetMsgRequest>(&context.requestEntity);
    GroupResponse<GGetMsgResult> resp;

    LOGT << "getMsg request received, uid: " << account->uid()
         << ", gid: " << req->gid << ", from: " << req->from 
         << ", to: " << req->to;

    bcm::Group groupInfo;
    dao::ErrorCode ec = m_groups->get(req->gid, groupInfo);
    if (ec == dao::ERRORCODE_NO_SUCH_DATA) {
        LOGE << "group not exist, gid: " << req->gid;
        resp.errorCode = 100405;
        resp.errorMsg = "no such gid";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "get group info error: " << ec << ", gid: " << req->gid;
        resp.errorCode = 1;
        resp.errorMsg = "query database error";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }

    GroupUser::Role role;
    ec = m_groupUsers->getMemberRole(req->gid, account->uid(), role);
    if (ec == dao::ERRORCODE_NO_SUCH_DATA) {
        LOGE << "user, uid: " << account->uid() 
             << ", is not a member of group, gid: " << req->gid;
        resp.errorCode = 100400;
        resp.errorMsg = "user does not belong to this group";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }
    if (ec != dao::ERRORCODE_SUCCESS) { 
        LOGE << "get member role error: " << ec << ", gid: " << req->gid 
             << ", uid: " << account->uid();
        resp.errorCode = 1;
        resp.errorMsg = "query database error";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }

    boost::optional<ClientVersion> cliver = boost::none;
    if (!account->devices().empty()) {
        cliver = account->devices().Get(0).clientversion();
    }

    bool isSupportRecall = (cliver && supportRecall(cliver.get()));
    std::vector<GroupMsg> msgs;
    ec = m_groupMsgs->batchGet(
        req->gid, req->from, req->to, 50, role, isSupportRecall, msgs);
    if (ec == dao::ERRORCODE_NO_SUCH_DATA) {
        // WARNING: don't treat this as an error
    } else if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "batch get error: " << ec << ", gid: " << req->gid 
             << ", from: " << req->from << ", to: " << req->to 
             << ", role: " << role << ", support recal: " << isSupportRecall;
        resp.errorCode = 1;
        resp.errorMsg = "query database error";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }

    if (!msgs.empty()) {
        resp.result.messages.reserve(msgs.size());
    }
    for (auto& m : msgs) {
        GMsgEntry ent;
        ent.gid = m.gid();
        ent.mid = m.mid();
        ent.fromUid = m.fromuid();
        ent.type = m.type();
        ent.text = m.text();
        ent.createTime = m.createtime();
        if (GroupMsg::TYPE_CHAT == m.type() || 
                GroupMsg::TYPE_CHANNEL == m.type()) {
            ent.status = m.status();
            ent.atList = m.atlist();
        }
        ent.sourceExtra = m.sourceextra();
        resp.result.messages.emplace_back(std::move(ent));
    }

    resp.errorCode = 0;
    resp.errorMsg = "success";
    resp.result.gid = req->gid;

    context.responseEntity = resp;
}

void GroupMsgController::ackMsg(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(groupMessageService, 
        groupMsgAckTopic);
    Account* account = boost::any_cast<Account>(&context.authResult);
    GAckMsgRequest* req = 
        boost::any_cast<GAckMsgRequest>(&context.requestEntity);
    GroupResponse<GNullResult> resp;

    LOGT << "ackMsg request received, uid: " << account->uid()
         << ", gid: " << req->gid << ", ackMsg: " << req->lastMid;

    GroupUser::Role role;
    dao::ErrorCode ec = 
        m_groupUsers->getMemberRole(req->gid, account->uid(), role);
    if (ec == dao::ERRORCODE_NO_SUCH_DATA) {
        LOGE << "user, uid: " << account->uid() 
             << ", is not a member of group, gid: " << req->gid;
        resp.errorCode = 100400;
        resp.errorMsg = "user does not belong to this group";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "get member role error: " << ec << ", gid: " << req->gid 
             << ", uid: " << account->uid();
        resp.errorCode = 1;
        resp.errorMsg = "query database error";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }

    nlohmann::json data = {
        {"last_ack_mid", req->lastMid}
    };
    ec = m_groupUsers->update(req->gid, account->uid(), data);
    if (ec != dao::ERRORCODE_SUCCESS && dao::ERRORCODE_ALREADY_EXSITED != ec) {
        LOGE << "update last_ack_mid error: " << ec << ", gid: " << req->gid
             << ", uid: " << account->uid() << ", last mid: " << req->lastMid;
        resp.errorCode = 1;
        resp.errorMsg = "database error";
        marker.setReturnCode(resp.errorCode);
    } else {
        LOGI << "last_ack_mid updated, gid: " << req->gid 
             << ", uid: " << account->uid() << ", last mid: " << req->lastMid;
        resp.errorCode = 0;
    }

    context.responseEntity = resp;
}

void GroupMsgController::queryLastMid(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(groupMessageService, 
        groupQueryMaxReadMidTopic);
    Account* account = boost::any_cast<Account>(&context.authResult);
    GroupResponse<GQueryLastMidResult> resp;

    LOGT << "queryLastMid request received, uid: " << account->uid();

    std::vector<uint64_t> gids;
    dao::ErrorCode ec = m_groupUsers->getJoinedGroups(account->uid(), gids);
    if (ec == dao::ERRORCODE_NO_SUCH_DATA) {
        LOGE << "get joined groups issue no data: " << ec << ", uid: "
             << account->uid();
        resp.errorCode = 110030;
        resp.errorMsg = "no such data";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;

    } else if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "get joined groups error: " << ec << ", uid: " 
             << account->uid();
        resp.errorCode = 110031;
        resp.errorMsg = "database error";
        context.responseEntity = resp;
        marker.setReturnCode(resp.errorCode);
        return;
    }

    resp.errorCode = 0;
    resp.errorMsg = "success";
    
    if (!gids.empty()) {
        std::vector<dao::UserGroupEntry> entries;
        ec = m_groupUsers->getGroupDetailByGidBatch(gids, account->uid(), entries);
        if (ec == dao::ERRORCODE_NO_SUCH_DATA) {
            LOGE << "getGroupDetailByGidBatch issue no data, uid: "
                 << account->uid() << ", error code: " << ec;
            resp.errorCode = 110030;
            resp.errorMsg = "no such data";
            context.responseEntity = resp;
            marker.setReturnCode(resp.errorCode);
            return;

        } else if (dao::ERRORCODE_SUCCESS != ec) {
            LOGE << "error invoking getGroupDetailByGidBatch, uid: "
                 << account->uid() << ", error code: " << ec;
            resp.errorCode = 110031;
            resp.errorMsg = "database error";
            context.responseEntity = resp;
            marker.setReturnCode(resp.errorCode);
            return;
        }
        for (const auto& itEntries : entries) {
            GLastMidEntry ent;
            ent.gid = itEntries.group.gid();
            ent.lastMid = itEntries.group.lastmid();
            ent.lastAckMid = itEntries.user.lastackmid();
            resp.result.groups.emplace_back(std::move(ent));
        }
    }

    context.responseEntity = resp;
}


void GroupMsgController::queryUids(HttpContext& context)
{
    context.response.result(http::status::ok);
}

void GroupMsgController::sendGroupChatMsg(const GroupMsg& msg)
{
    ExecTimeAndReturnCodeMarker marker(groupMessageService, 
        groupPublishRedisTopic);
    // add a new field "from_uid_extra" here to filling the "from_uid" for generating push message later
    // the original field "from_uid" will just be used to check whether the push target is the sender himself
    std::string fromUidExtra = m_encryptSenderConfig.plainUidSupport ? msg.fromuid() : "";
    nlohmann::json j = nlohmann::json::object({
        {"type",           msg.type()},
        {"gid",            msg.gid()},
        {"mid",            msg.mid()},
        {"status",         msg.status()},
        {"text",           msg.text()},
        {"from_uid",       msg.fromuid()},
        {"create_time",    msg.createtime()},
        {"at_list",        msg.atlist()},
        {"at_all",         msg.atall()},
        {"source_extra",   msg.sourceextra()},
        {"from_uid_extra", fromUidExtra}
    });

    std::string topic = "group_" + std::to_string(msg.gid());
    std::string pubMsg = j.dump();
    OnlineRedisManager::Instance()->publish(topic, pubMsg, [topic, pubMsg](int status, const redis::Reply& reply) {
        if (REDIS_OK != status || !reply.isInteger()) {
            LOGE << "failed to publish, channel: " << topic << ", status: " << status << ", msg: " << pubMsg;
            return;
        }
        LOGT << "is published to '" << topic << "', msg: " << pubMsg;
    });
}

int GroupMsgController::getSourceExtra(const std::string& uid, const std::string& publicKey, std::string& sSourceExtra)
{
    std::string encrypted;
    uint32_t version;
    std::string iv;
    std::string ephemeralPubkey;
    int ret = -1;
    if ((ret = SenderUtils::encryptSender(uid, publicKey, version, iv, ephemeralPubkey, encrypted)) != 0) {
        return ret;
    }
    nlohmann::json sourceExtra = nlohmann::json::object(
            {
                    {"version",         version},
                    {"groupMsgPubkey",  publicKey},
                    {"ephemeralPubkey", Base64::encode(ephemeralPubkey)},
                    {"iv",              Base64::encode(iv)},
                    {"source",          Base64::encode(encrypted)}
            });
    sSourceExtra = Base64::encode(sourceExtra.dump());
    return ret;
}

} // namespace bcm

