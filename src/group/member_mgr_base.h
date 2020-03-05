#pragma once

#include <vector>
#include <map>
#include <set>
#include <string>
#include <shared_mutex>
#include "dispatcher/dispatch_address.h"

struct event_base;

namespace bcm {
namespace dao {
class GroupUsers;
} // namespace dao
} // namespace bcm

namespace bcm {

class IoCtxPool;

class MemberMgrBase {
public:
    typedef std::shared_ptr<dao::GroupUsers> GroupUsersDaoPtr;
    typedef std::set<DispatchAddress> UserSet;
    typedef std::vector<DispatchAddress> UserList;

    MemberMgrBase(GroupUsersDaoPtr groupUsers, IoCtxPool& ioCtxPool);
    virtual ~MemberMgrBase() {}

    void getGroupMembers(uint64_t gid, UserList& users) const;
    void getGroupMembers(uint64_t gid, UserSet& users) const;

    void removeGroupMembers(const UserSet& users, uint64_t gid);
    void removeMemberForGroups(const DispatchAddress& user,
                               const std::vector<uint64_t>& gids);

    bool isGroupExist(uint64_t gid) const;

    IoCtxPool& ioCtxPool();

protected:
    void removeGroupMemberNoLock(const DispatchAddress& user, uint64_t gid);

protected:
    GroupUsersDaoPtr m_groupUsersDao;
    std::map<uint64_t, UserSet> m_groupMembers;
    mutable std::shared_timed_mutex m_memberMutex;
    IoCtxPool& m_ioCtxPool;
};

} // namespace bcm
