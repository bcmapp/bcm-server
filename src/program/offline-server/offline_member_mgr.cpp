#include "offline_member_mgr.h"

namespace bcm {

OfflineMemberMgrBase::OfflineMemberMgrBase(GroupUsersDaoPtr groupUsersDao, 
                             IoCtxPool& ioCtxPool)
    : m_groupUsersDao(groupUsersDao), m_ioCtxPool(ioCtxPool)
{
}

void OfflineMemberMgrBase::getGroupMembers(uint64_t gid, UidList& uids) const
{
    std::shared_lock<std::shared_timed_mutex> l(m_memberMutex);
    const auto it = m_groupMembers.find(gid);
    if (it != m_groupMembers.end()) {
        uids.reserve(it->second.size());
        for (const std::string& uid : it->second) {
            uids.emplace_back(uid);
        }
    }
}

void OfflineMemberMgrBase::getGroupSubscribers(uint64_t gid, UidList& uids) const
{
    std::shared_lock<std::shared_timed_mutex> l(m_memberMutex);
    const auto it = m_groupSubscribers.find(gid);
    if (it != m_groupSubscribers.end()) {
        uids.reserve(it->second.size());
        for (const std::string& uid : it->second) {
            uids.emplace_back(uid);
        }
    }
}


bool OfflineMemberMgrBase::isGroupExist(uint64_t gid) const
{

    std::shared_lock<std::shared_timed_mutex> l(m_memberMutex);
    if (m_groupMembers.find(gid) != m_groupMembers.end()) {
        return true;
    }

    if (m_groupSubscribers.find(gid) != m_groupSubscribers.end()) {
        return true;
    }

    return false;
}

void OfflineMemberMgrBase::getGroupMembers(uint64_t gid, UidSet& uids) const
{
    std::shared_lock<std::shared_timed_mutex> l(m_memberMutex);
    const auto it = m_groupMembers.find(gid);
    if (it != m_groupMembers.end()) {
        uids.insert(it->second.begin(), it->second.end());
    }
}

/* void OfflineMemberMgrBase::getGroupMembers(uint64_t gid, UidSet::size_type size, UidSet& uids) const
{
    std::shared_lock<std::shared_timed_mutex> l(m_memberMutex);
    const auto it = m_groupMembers.find(gid);
    if (it == m_groupMembers.end()) {
        return;
    }
    if (size == 0) {
        uids.insert(it->second.begin(), it->second.end());
        return;
    }
    for (const auto& item : it->second) {
        uids.emplace(item);
        if (--size == 0) {
            break;
        }
    }

} */

void OfflineMemberMgrBase::getGroupSubscribers(uint64_t gid, UidSet& uids) const
{
    std::shared_lock<std::shared_timed_mutex> l(m_memberMutex);
    const auto it = m_groupSubscribers.find(gid);
    if (it != m_groupSubscribers.end()) {
        uids.insert(it->second.begin(), it->second.end());
    }
}

void OfflineMemberMgrBase::removeGroupMember(const std::string& uid, uint64_t gid)
{
    std::unique_lock<std::shared_timed_mutex> l(m_memberMutex);
    removeGroupMemberNoLock(uid, gid);
}

void OfflineMemberMgrBase::removeMemberForGroups(const std::string& uid,
                                          const std::vector<uint64_t>& gids)
{
    std::unique_lock<std::shared_timed_mutex> l(m_memberMutex);
    for (auto gid : gids) {
        removeGroupMemberNoLock(uid, gid);
    }
}

void OfflineMemberMgrBase::removeGroupSubscriber(const std::string& uid, uint64_t gid)
{
    std::unique_lock<std::shared_timed_mutex> l(m_memberMutex);
    removeGroupSubscriberNoLock(uid, gid);
}

void OfflineMemberMgrBase::removeSubscriberForGroups(
    const std::string& uid, const std::vector<uint64_t>& gids)
{
    std::unique_lock<std::shared_timed_mutex> l(m_memberMutex);
    for (auto gid : gids) {
        removeGroupSubscriberNoLock(uid, gid);
    }
}

void OfflineMemberMgrBase::removeGroupMemberNoLock(const std::string& uid, 
                                            uint64_t gid)
{
    auto it = m_groupMembers.find(gid);
    if (it != m_groupMembers.end()) {
        it->second.erase(uid);
        if (it->second.empty()) {
            m_groupMembers.erase(gid);
        }
    }
}

void OfflineMemberMgrBase::removeGroupSubscriberNoLock(const std::string& uid, 
                                                uint64_t gid)
{
    auto it = m_groupSubscribers.find(gid);
    if (it != m_groupSubscribers.end()) {
        it->second.erase(uid);
        if (it->second.empty()) {
            m_groupSubscribers.erase(gid);
        }
    }
}

IoCtxPool& OfflineMemberMgrBase::ioCtxPool() {
    return m_ioCtxPool;
}

} // namespace bcm
