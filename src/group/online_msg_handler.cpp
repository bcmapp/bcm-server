#include "online_msg_handler.h"
#include "group_msg_sub.h"
#include "online_msg_member_mgr.h"
#include "io_ctx_pool.h"
#include "dispatcher/dispatch_manager.h"
#include "utils/log.h"

#include "proto/dao/device.pb.h"
#include "proto/dao/group_msg.pb.h"
#include "proto/group/message.pb.h"
#include "redis/redis_manager.h"
#include "config/group_store_format.h"

namespace bcm {

static const std::string groupMessageService = "bcm_gmessager";
static const std::string receivedGroupMessageFromRedis = 
    "received_group_message_from_redis";

// -----------------------------------------------------------------------------
// Section: utility functions for building various messages
// -----------------------------------------------------------------------------
static void buildOutMessage(GroupMsg_Type type, const std::string& body, 
                            std::string& out)
{
    GroupMsgOut msgOut;
    msgOut.set_type(type);
    msgOut.set_body(body);
    msgOut.SerializeToString(&out);
}

static void buildChatMessageBody(const nlohmann::json& j, std::string& out)
{
    GroupChatMsg body;
    body.set_gid(j.at("gid").get<uint64_t>());
    body.set_mid(j.at("mid").get<uint64_t>());
    std::string v;
    if (j.find("from_uid_extra") == j.end()) {
        v = j.at("from_uid").get<std::string>();
    } else {
        v = j.at("from_uid_extra").get<std::string>();
    }
    body.set_from_uid(v);
    body.set_text(j.at("text").get<std::string>());
    body.set_status(GroupMsg_Status(j.at("status").get<int>()));
    body.set_create_time(j.at("create_time").get<uint64_t>());
    body.mutable_content()->set_at_all(j.at("at_all").get<int>() == 1);

    std::string atList = j.at("at_list").get<std::string>();
    if (!atList.empty()) {
        nlohmann::json arr = nlohmann::json::parse(atList);
        std::vector<std::string> uids;
        arr.get_to(uids);
        for (auto& uid : uids) {
            body.mutable_content()->add_at_list(uid);
        }
    }
    std::string se = "";
    if (j.find("source_extra") != j.end()) {
        se = j.at("source_extra").get<std::string>();
    }
    body.set_source_extra(se);
    body.SerializeToString(&out);
}

static void buildChatMessage(const nlohmann::json& j, std::string& out, bool isNoise)
{
    std::string outBody;
    buildChatMessageBody(j, outBody);
    GroupMsg::Type t = GroupMsg::TYPE_CHAT;
    if (isNoise) {
        t = GroupMsg::TYPE_NOISE;
    }
    buildOutMessage(t, outBody, out);
}

static void buildChannelMessage(const nlohmann::json& j, std::string& out, bool isNoise)
{
    std::string outBody;
    buildChatMessageBody(j, outBody);
    GroupMsg::Type t = GroupMsg::TYPE_CHANNEL;
    if (isNoise) {
        t = GroupMsg::TYPE_NOISE;
    }
    buildOutMessage(t, outBody, out);
}

static void buildInfoUpdateMessage(const nlohmann::json& j, std::string& out, bool isNoise)
{
    GroupInfoUpdate body;
    body.set_gid(j.at("gid").get<uint64_t>());
    body.set_mid(j.at("mid").get<uint64_t>());
    body.set_from_uid(j.at("from_uid").get<std::string>());

    std::string text = j.at("text").get<std::string>();
    if (text.empty()) {
        LOGE << "text is empty";
        return;
    }
    nlohmann::json textObj = nlohmann::json::parse(text);

    body.set_last_mid(textObj.at("last_mid").get<uint64_t>());
    body.set_intro(textObj.at("intro").get<std::string>());
    body.set_broadcast(textObj.at("broadcast").get<int>());
    body.set_create_time(textObj.at("create_time").get<uint64_t>());
    body.set_update_time(textObj.at("update_time").get<uint64_t>());
    body.set_channel(textObj.at("channel").get<std::string>());

    jsonable::toString(textObj, "name", *body.mutable_name(), jsonable::OPTIONAL); // TODO: deprecated
    jsonable::toString(textObj, "icon", *body.mutable_icon(), jsonable::OPTIONAL); // TODO: deprecated
    jsonable::toString(textObj, "encrypted_name", *body.mutable_encryptedname(), jsonable::OPTIONAL);
    jsonable::toString(textObj, "encrypted_icon", *body.mutable_encryptedicon(), jsonable::OPTIONAL);

    std::string outBody;
    body.SerializeToString(&outBody);
    GroupMsg::Type t = GroupMsg::TYPE_INFO_UPDATE;
    if (isNoise) {
        t = GroupMsg::TYPE_NOISE;
    }
    buildOutMessage(t, outBody, out);
}

static void buildGroupSwitchGroupKeysMessage(const nlohmann::json& j, std::string& out, bool isNoise)
{
    GroupSwitchGroupKeys body;
    body.set_gid(j.at("gid").get<uint64_t>());
    body.set_mid(j.at("mid").get<uint64_t>());
    body.set_from_uid(j.at("from_uid").get<std::string>());

    std::string text = j.at("text").get<std::string>();
    if (text.empty()) {
        LOGE << "text is empty";
        return;
    }
    nlohmann::json textObj = nlohmann::json::parse(text);
    body.set_version(textObj.at("version").get<uint64_t>());

    std::string outBody;
    body.SerializeToString(&outBody);
    GroupMsg::Type t = GroupMsg::TYPE_SWITCH_GROUP_KEYS;
    if (isNoise) {
        t = GroupMsg::TYPE_NOISE;
    }
    buildOutMessage(t, outBody, out);
}

static void buildGroupUpdateGroupKeysRequestMessage(const nlohmann::json& j, std::string& out, bool isNoise)
{
    GroupUpdateGroupKeysRequest body;
    body.set_gid(j.at("gid").get<uint64_t>());
    body.set_mid(j.at("mid").get<uint64_t>());
    body.set_from_uid(j.at("from_uid").get<std::string>());

    std::string text = j.at("text").get<std::string>();
    if (text.empty()) {
        LOGE << "text is empty";
        return;
    }
    nlohmann::json textObj = nlohmann::json::parse(text);
    body.set_keysmode(textObj.at("group_keys_mode").get<int32_t>());

    std::string outBody;
    body.SerializeToString(&outBody);
    GroupMsg::Type t = GroupMsg::TYPE_UPDATE_GROUP_KEYS_REQUEST;
    if (isNoise) {
        t = GroupMsg::TYPE_NOISE;
    }
    buildOutMessage(t, outBody, out);
}

static void buildMemberUpdateMessage(const nlohmann::json& j, std::string& out, bool isNoise)
{
    GroupMemberUpdate body;
    body.set_gid(j.at("gid").get<uint64_t>());
    body.set_mid(j.at("mid").get<uint64_t>());
    body.set_from_uid(j.at("from_uid").get<std::string>());

    std::string text = j.at("text").get<std::string>();
    if (text.empty()) {
        LOGE << "text is empty";
        return;
    }
    nlohmann::json textObj = nlohmann::json::parse(text);
    body.set_action(textObj.at("action").get<int>());
    const nlohmann::json& arr = textObj.at("members");
    for (nlohmann::json::const_iterator it = arr.begin(); 
            it != arr.end(); ++it) {
        GroupMemberUpdate::GroupMember* m = body.add_members();
        m->set_uid(it->at("uid").get<std::string>());
        m->set_nick(it->at("nick").get<std::string>());
        m->set_role(it->at("role").get<int>());
    }

    std::string outBody;
    body.SerializeToString(&outBody);
    GroupMsg::Type t = GroupMsg::TYPE_MEMBER_UPDATE;
    if (isNoise) {
        t = GroupMsg::TYPE_NOISE;
    }
    buildOutMessage(t, outBody, out);
}

static void buildRecallMessage(const nlohmann::json& j, std::string& out, bool isNoise)
{
    GroupRecallMsg body;
    body.set_gid(j.at("gid").get<uint64_t>());
    body.set_mid(j.at("mid").get<uint64_t>());
    std::string v;
    if (j.find("from_uid_extra") == j.end()) {
        v = j.at("from_uid").get<std::string>();
    } else {
        v = j.at("from_uid_extra").get<std::string>();
    }
    body.set_from_uid(v);

    std::string text = j.at("text").get<std::string>();
    if (text.empty()) {
        LOGE << "text is empty";
        return;
    }
    nlohmann::json textObj = nlohmann::json::parse(text);
    body.set_recalled_mid(textObj.at("recalled_mid").get<uint64_t>());
    std::string se = "";
    if (j.find("source_extra") != j.end()) {
        se = j.at("source_extra").get<std::string>();
    }
    body.set_source_extra(se);

    std::string outBody;
    body.SerializeToString(&outBody);
    GroupMsg::Type t = GroupMsg::TYPE_RECALL;
    if (isNoise) {
        t = GroupMsg::TYPE_NOISE;
    }
    buildOutMessage(t, outBody, out);
}

// -----------------------------------------------------------------------------
// Section: OnlineMsgHandler
// -----------------------------------------------------------------------------
OnlineMsgHandler::OnlineMsgHandler(std::shared_ptr<DispatchManager> dispatchMgr, 
                                   OnlineMsgMemberMgr& memberMgr,
                                   const NoiseConfig& cfg)
    : m_dispatchMgr(dispatchMgr), m_memberMgr(memberMgr), m_noiseCfg(cfg), m_lastNoiseUid("")
{
}

OnlineMsgHandler::~OnlineMsgHandler()
{
}

void OnlineMsgHandler::handleMessage(const std::string& chan, 
                                     const nlohmann::json& msgObj)
{
    if (GroupMsgSub::isGroupMessageChannel(chan)) {
        uint64_t gid = msgObj.at("gid").get<uint64_t>();
        IoCtxPool& pool = m_memberMgr.ioCtxPool();
        IoCtxPool::io_context_ptr ioc = pool.getIoCtxByGid(gid);
        if (ioc != nullptr) {
            ioc->post([=]() {
                std::vector<DispatchManager::GroupMessages> messages;

                OnlineMsgMemberMgr::UserSet onlineUsers;
                std::shared_ptr<std::string> msg = std::make_shared<std::string>();
                // create group messages and target uids
                handleGroupMessage(chan, msgObj, onlineUsers, *msg);

                std::shared_ptr<OnlineMsgMemberMgr::UserSet> addrs = std::make_shared<OnlineMsgMemberMgr::UserSet>();
                for (const auto& user : onlineUsers) {
                    addrs->emplace(user);
                }
                if (!addrs->empty()) {
                    messages.emplace_back(addrs, msg);
                }

                std::shared_ptr<std::string> noiseMsg = std::make_shared<std::string>();
                std::shared_ptr<OnlineMsgMemberMgr::UserSet> noiseAddrs = std::make_shared<OnlineMsgMemberMgr::UserSet>();
                if (m_noiseCfg.enabled) {
                    try {
                        OnlineMsgMemberMgr::UserSet noiseUsers;
                        // generate noise messages
                        generateNoiseForGroupMessage(chan, msgObj, onlineUsers, noiseUsers, *noiseMsg);
                        for (const auto& user : noiseUsers) {
                            noiseAddrs->emplace(user);
                        }
                        if (!noiseAddrs->empty()) {
                            messages.emplace_back(noiseAddrs, noiseMsg);
                        }
                    } catch (const std::exception& ex) {
                        LOGE << "failed to generate noise message, message: " << msgObj.dump() << ", error: " << ex.what();
                    }
                }
                // send messages
                m_dispatchMgr->sendGroupMessage(messages);
            });
        }
    }
}

void OnlineMsgHandler::handleGroupMessage(const std::string& chan,
                                          const nlohmann::json& msgObj,
                                          OnlineMsgMemberMgr::UserSet& targetUsers,
                                          std::string& message)
{
    boost::ignore_unused(chan);
    GroupMsg_Type msgType = GroupMsg_Type(msgObj.at("type").get<int>());
    uint64_t gid = msgObj.at("gid").get<uint64_t>();

    switch (msgType) {
    case GroupMsg::TYPE_CHAT:
        buildChatMessage(msgObj, message, false);
        m_memberMgr.getGroupMembers(gid, targetUsers);
        break;
    case GroupMsg::TYPE_CHANNEL:
        buildChannelMessage(msgObj, message, false);
        m_memberMgr.getGroupMembers(gid, targetUsers);
        break;
    case GroupMsg::TYPE_INFO_UPDATE:
        buildInfoUpdateMessage(msgObj, message, false);
        m_memberMgr.getGroupMembers(gid, targetUsers);
        break;
    case GroupMsg::TYPE_MEMBER_UPDATE:
    {
        buildMemberUpdateMessage(msgObj, message, false);
        m_memberMgr.getGroupMembers(gid, targetUsers);
    
        try {
            std::string text = msgObj.at("text").get<std::string>();
            if (text.empty()) {
                LOGE << "text is empty, channel: " << chan << ", json: " << msgObj.dump();
                break;
            }
            nlohmann::json textObj = nlohmann::json::parse(text);
            const nlohmann::json& arr = textObj.at("members");
            for (nlohmann::json::const_iterator it = arr.begin(); it != arr.end(); ++it) {
                std::string uid = it->at("uid").get<std::string>();

                auto users = m_memberMgr.getOnlineUsers(uid);

                if (users.empty()) {
                    continue;
                }

                for(const auto& it : users) {
                    if (targetUsers.find(it) == targetUsers.end()) {
                        targetUsers.insert(it);
                    }
                }
            }
        } catch (std::exception& e) {
            LOGE << "handle message json false, from channel: " << chan
                 << ", message: " << msgObj.dump()
                 << ", exception caught: " << e.what();
        }
        break;
    }
    case GroupMsg::TYPE_RECALL:
        buildRecallMessage(msgObj, message, false);
        m_memberMgr.getGroupMembers(gid, targetUsers);
        break;
    case GroupMsg::TYPE_SWITCH_GROUP_KEYS:
        buildGroupSwitchGroupKeysMessage(msgObj, message, false);
        m_memberMgr.getGroupMembers(gid, targetUsers);
        break;
    case GroupMsg::TYPE_UPDATE_GROUP_KEYS_REQUEST:
        buildGroupUpdateGroupKeysRequestMessage(msgObj, message, false);
        m_memberMgr.getGroupMembers(gid, targetUsers);
        break;
    default:
        LOGE << "received unkown message: " << msgObj.dump();
        break;
    }

    if (targetUsers.empty()) {
        return;
    }

    // TODO
    bcm::GroupUserMessageIdInfo redisDbUserMsgInfo;
    redisDbUserMsgInfo.last_mid = msgObj.at("mid").get<uint64_t>();
    std::string hkey = REDISDB_KEY_PREFIX_GROUP_USER_INFO + std::to_string(gid);
    std::vector<HField> hvalues;
    std::string val = redisDbUserMsgInfo.to_string();
    for (auto& u : targetUsers) {
        // for now offline push only support master device
        if (u.getDeviceid() == Device::MASTER_ID) {
            hvalues.emplace_back(u.getUid(), val);
        }
    }
    if (!RedisDbManager::Instance()->hmset(gid, hkey, hvalues)) {
        LOGE << "failed to hmset users' mid to redis db, message: gid " << gid << ", mid " << redisDbUserMsgInfo.last_mid;
    }
}

void OnlineMsgHandler::generateNoiseForGroupMessage(const std::string& chan,
                                                    const nlohmann::json& msgObj,
                                                    const OnlineMsgMemberMgr::UserSet& onlineUsers,
                                                    OnlineMsgMemberMgr::UserSet& targetUsers,
                                                    std::string& message)
{
    boost::ignore_unused(chan);
    GroupMsg_Type msgType = GroupMsg_Type(msgObj.at("type").get<int>());
    uint64_t gid = msgObj.at("gid").get<uint64_t>();

    switch (msgType) {
        case GroupMsg::TYPE_CHAT:
            buildChatMessage(msgObj, message, true);
            break;
        case GroupMsg::TYPE_CHANNEL:
            buildChannelMessage(msgObj, message, true);
            break;
        case GroupMsg::TYPE_INFO_UPDATE:
            buildInfoUpdateMessage(msgObj, message, true);
            break;
        case GroupMsg::TYPE_MEMBER_UPDATE:
            buildMemberUpdateMessage(msgObj, message, true);
            break;
        case GroupMsg::TYPE_RECALL:
            buildRecallMessage(msgObj, message, true);
            break;
        default:
            LOGE << "received unkown message: " << msgObj.dump();
            return;
    }

    pickNoiseReceivers(gid, onlineUsers, targetUsers);
}

void OnlineMsgHandler::pickNoiseReceivers(uint64_t gid,
                                          const OnlineMsgMemberMgr::UserSet& onlineUids,
                                          OnlineMsgMemberMgr::UserSet& targetUids)
{
    targetUids.clear();
    if (!m_noiseCfg.enabled) {
        return;
    }
    size_t nReceivers = m_noiseCfg.percentage * onlineUids.size();
    if (nReceivers == 0 && onlineUids.size() > 0) {
        nReceivers = 1;
    }
    if (nReceivers == 0) {
        targetUids.clear();
        return;
    }

    std::string uid("");
    m_memberMgr.getOnlineUsers(m_lastNoiseUid,
                               gid,
                               m_noiseCfg.iosSupportedVersion,
                               m_noiseCfg.androidSupportedVersion,
                               nReceivers,
                               targetUids,
                               uid,
                               m_dispatchMgr);
    if (!targetUids.empty()) {
        // there is only one thread for online group message pushing,
        // so we don't have to do synchoronization for multithreading
        m_lastNoiseUid = uid;
    }
}

} // namespace bcm

