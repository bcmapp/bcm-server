#pragma once

#include "member_mgr_base.h"
#include "group_msg_sub.h"

#include <string>
#include <set>
#include <vector>
#include <shared_mutex>
#include "dispatcher/dispatch_manager.h"

namespace bcm {

class OnlineMsgMemberMgr : public MemberMgrBase {
public:
    typedef MemberMgrBase::GroupUsersDaoPtr GroupUsersDaoPtr;
    typedef std::map<std::string, UserSet> UserMap;
    typedef std::set<DispatchAddress> UserSet;
    typedef std::vector<DispatchAddress> UserList;

    OnlineMsgMemberMgr(GroupUsersDaoPtr groupUsersDao, IoCtxPool& pool);

    void handleUserOnline(const DispatchAddress& user);
    void handleUserOffline(const DispatchAddress& user);
    void handleUserEnterGroup(const std::string& uid, uint64_t gid);
    void handleUserLeaveGroup(const std::string& uid, uint64_t gid);


    void addGroupMsgSubHandler(GroupMsgSub* handler);

    void getOnlineUsers(const std::string& start,
                        uint64_t excludedGid,
                        uint64_t iosVer,
                        uint64_t androidVer,
                        size_t count,
                        UserSet& result,
                        std::string& lastNoiseUid,
                        std::shared_ptr<DispatchManager> pDispatchMgr);

    UserSet getOnlineUsers(const std::string& uid)
    {
        std::shared_lock<std::shared_timed_mutex> l(m_onlineUsersMtx);
        const auto& it = m_onlineUsers.find(uid);
        if (it != m_onlineUsers.end()) {
            return it->second;
        }
        return UserSet{};
    }


private:
    void doHandleUserOnline(const DispatchAddress& user);
    void doHandleUserOffline(const DispatchAddress& user);
    void doHandleUserEnterGroup(const std::string& uid, uint64_t gid);
    void doHandleUserLeaveGroup(const std::string& uid, uint64_t gid);

private:
    UserMap m_onlineUsers;
    std::shared_timed_mutex m_onlineUsersMtx;

    GroupMsgSub* m_groupSub;
};

} // namespace bcm
