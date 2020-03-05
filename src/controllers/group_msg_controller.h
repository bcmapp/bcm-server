#pragma once

#include "http/http_router.h"
#include "dao/groups.h"
#include "dao/group_users.h"
#include "dao/group_msgs.h"
#include "config/encrypt_sender.h"
#include "config/size_check_config.h"

namespace bcm {
class RedisClientSync;
class GroupMsgService;
class GroupMsg;
} // namespace bcm


namespace bcm {

class GroupMsgController 
    : public HttpRouter::Controller
    , public std::enable_shared_from_this<GroupMsgController> {

public:
    GroupMsgController(std::shared_ptr<GroupMsgService> groupMsgService,
                       const EncryptSenderConfig& cfg,
                       const SizeCheckConfig& scCfg);

    void addRoutes(HttpRouter& router) override;

    void sendMsg(HttpContext& context);
    void recallMsg(HttpContext& context);
    void getMsg(HttpContext& context);
    void ackMsg(HttpContext& context);
    void queryLastMid(HttpContext& context);
    void queryUids(HttpContext& context);

private:
    void sendGroupChatMsg(const GroupMsg& msg);
    int getSourceExtra(const std::string& uid, const std::string& publicKey, std::string& sSourceExtra);

private:
    std::shared_ptr<GroupMsgService> m_groupMsgService;

    std::shared_ptr<dao::Groups> m_groups;
    std::shared_ptr<dao::GroupUsers> m_groupUsers;
    std::shared_ptr<dao::GroupMsgs> m_groupMsgs;
    EncryptSenderConfig m_encryptSenderConfig;
    SizeCheckConfig m_sizeCheckConfig;
};

} // namespace bcm