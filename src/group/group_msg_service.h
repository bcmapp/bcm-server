#pragma once

#include <memory>

#include "config/redis_config.h"
#include "config/noise_config.h"
#include "config/group_store_format.h"
#include "dispatcher/dispatch_address.h"
#include "group/online_msg_member_mgr.h"

#include <nlohmann/json.hpp>

namespace bcm {
namespace push {
class Service;
} // namespace push
} // namespace bcm

namespace bcm {

class DispatchManager;
class AccountsManager;
class OnlineUserRegistry;
class GroupMsgServiceImpl;

class GroupMsgService : public std::enable_shared_from_this<GroupMsgService> {
public:
    GroupMsgService(const RedisConfig redisCfg,
                    std::shared_ptr<DispatchManager> dispatchMgr,
                    const NoiseConfig& noiseCfg);
    virtual ~GroupMsgService();

    void addRegKey(const std::string& key);
    void notifyUserOnline(const DispatchAddress& user);
    void notifyUserOffline(const DispatchAddress& user);

    void updateRedisdbOfflineInfo(uint64_t gid, uint64_t mid, GroupMultibroadMessageInfo& groupMultibroadInfo);

    virtual void getLocalOnlineGroupMembers(uint64_t gid, uint32_t count, OnlineMsgMemberMgr::UserList& users);

private:
    GroupMsgServiceImpl* m_pImpl;
    GroupMsgServiceImpl& m_impl;
};

} // namespace bcm
