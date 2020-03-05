#pragma once

#include <vector>
#include <map>
#include <set>
#include <string>
#include <shared_mutex>

struct event_base;

namespace bcm {
namespace dao {
class GroupUsers;
} // namespace dao
} // namespace bcm

namespace bcm {

class IoCtxPool;

class OfflineMemberMgrBase {
public:
    typedef std::shared_ptr<dao::GroupUsers> GroupUsersDaoPtr;
    typedef std::set<std::string> UidSet;
    typedef std::vector<std::string> UidList;

    OfflineMemberMgrBase(GroupUsersDaoPtr groupUsers, IoCtxPool& ioCtxPool);
    virtual ~OfflineMemberMgrBase() {}

    void getGroupMembers(uint64_t gid, UidList& uids) const;
    void getGroupSubscribers(uint64_t gid, UidList& uids) const;
    void getGroupMembers(uint64_t gid, UidSet& uids) const;
    // void getGroupMembers(uint64_t gid, UidSet::size_type size, UidSet& uids) const;
    void getGroupSubscribers(uint64_t gid, UidSet& uids) const;
    void removeGroupMember(const std::string& uid, uint64_t gid);
    void removeMemberForGroups(const std::string& uid,
                               const std::vector<uint64_t>& gids);
    void removeGroupSubscriber(const std::string& uid, uint64_t gid);
    void removeSubscriberForGroups(const std::string& uid, 
                                   const std::vector<uint64_t>& gids);

    bool isGroupExist(uint64_t gid) const;

    IoCtxPool& ioCtxPool();

protected:
    void removeGroupMemberNoLock(const std::string& uid, uint64_t gid);
    void removeGroupSubscriberNoLock(const std::string& uid, uint64_t gid);

protected:
    GroupUsersDaoPtr m_groupUsersDao;
    std::map<uint64_t, UidSet> m_groupMembers;
    std::map<uint64_t, UidSet> m_groupSubscribers;
    mutable std::shared_timed_mutex m_memberMutex;
    IoCtxPool& m_ioCtxPool;
};

} // namespace bcm
