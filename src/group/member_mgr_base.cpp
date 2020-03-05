#include "member_mgr_base.h"

namespace bcm {

MemberMgrBase::MemberMgrBase(GroupUsersDaoPtr groupUsersDao, 
                             IoCtxPool& ioCtxPool)
    : m_groupUsersDao(groupUsersDao), m_ioCtxPool(ioCtxPool)
{
}

void MemberMgrBase::getGroupMembers(uint64_t gid, UserList& users) const
{
    std::shared_lock<std::shared_timed_mutex> l(m_memberMutex);
    const auto it = m_groupMembers.find(gid);
    if (it != m_groupMembers.end()) {
        users.reserve(it->second.size());
        for (auto& user : it->second) {
            users.emplace_back(user);
        }
    }
}

bool MemberMgrBase::isGroupExist(uint64_t gid) const
{

    std::shared_lock<std::shared_timed_mutex> l(m_memberMutex);
    if (m_groupMembers.find(gid) != m_groupMembers.end()) {
        return true;
    }

    return false;
}

void MemberMgrBase::getGroupMembers(uint64_t gid, UserSet& uids) const
{
    std::shared_lock<std::shared_timed_mutex> l(m_memberMutex);
    const auto it = m_groupMembers.find(gid);
    if (it != m_groupMembers.end()) {
        uids.insert(it->second.begin(), it->second.end());
    }
}

/* void MemberMgrBase::getGroupMembers(uint64_t gid, UserSet::size_type size, UserSet& uids) const
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

void MemberMgrBase::removeGroupMembers(const UserSet& users, uint64_t gid)
{
    std::unique_lock<std::shared_timed_mutex> l(m_memberMutex);
    for (auto& user : users) {
        removeGroupMemberNoLock(user, gid);
    }
}

void MemberMgrBase::removeMemberForGroups(const DispatchAddress& user,
                                          const std::vector<uint64_t>& gids)
{
    std::unique_lock<std::shared_timed_mutex> l(m_memberMutex);
    for (auto gid : gids) {
        removeGroupMemberNoLock(user, gid);
    }
}

void MemberMgrBase::removeGroupMemberNoLock(const DispatchAddress& user, 
                                            uint64_t gid)
{
    auto it = m_groupMembers.find(gid);
    if (it != m_groupMembers.end()) {
        it->second.erase(user);
        if (it->second.empty()) {
            m_groupMembers.erase(gid);
        }
    }
}

IoCtxPool& MemberMgrBase::ioCtxPool() {
    return m_ioCtxPool;
}

} // namespace bcm
