#pragma once

#include <string>
#include <shared_mutex>

#include "offline_member_mgr.h"


namespace bcm {

    const uint32_t     DB_RELOAD_GROUPUSER_INTERVAL     = 10;   // s

class GroupMemberMgr
    : public OfflineMemberMgrBase{

public:
    typedef OfflineMemberMgrBase::GroupUsersDaoPtr GroupUsersDaoPtr;
    typedef OfflineMemberMgrBase::UidSet UidSet;
    typedef OfflineMemberMgrBase::UidList UidList;

    GroupMemberMgr(GroupUsersDaoPtr groupUsersDao, IoCtxPool& ioCtxPool);

    void handleUserEnterGroup(const std::string& uid, uint64_t gid);
    void handleUserLeaveGroup(const std::string& uid, uint64_t gid);
    void handleUserMuteGroup(const std::string& uid, uint64_t gid);
    void handleUserUmuteGroup(const std::string& uid, uint64_t gid);

    void getUnmuteGroupMembers(uint64_t gid, UidSet& uids) const;
    void getUnmuteGroupSubscribers(uint64_t gid, UidSet& uids) const;
    bool loadGroupMembersFromDb(uint64_t gid);

    bool syncReloadGroupMembersFromDb(uint64_t gid);

    bool isMemberExists(const std::string& uid, uint64_t gid) const;

private:
    void doHandleUserEnterGroup(const std::string& uid, uint64_t gid);
    void doHandleUserLeaveGroup(const std::string& uid, uint64_t gid);
    void doHandleUserMuteGroup(const std::string& uid, uint64_t gid);
    void doHandleUserUnmuteGroup(const std::string& uid, uint64_t gid);

    void removeMutedMember(const std::string& uid, uint64_t gid);
    void removeMutedMemberNoLock(const std::string& uid, uint64_t gid);

private:
    std::map<uint64_t /* gid */, UidSet>    m_mutedMembers;
    std::map<uint64_t /* gid */, int64_t>   m_reloadTimes;
};

} // namespace bcm
