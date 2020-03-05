
#include "group_member_mgr.h"

#include <utils/jsonable.h>
#include <utils/time.h>
#include <utils/log.h>
#include "utils/libevent_utils.h"
#include "utils/thread_utils.h"

#include "../../group/io_ctx_pool.h"
#include "../../group/message_type.h"

#include "dao/group_users.h"
#include "utils/libevent_utils.h"

#include "proto/dao/group_user.pb.h"

namespace bcm {

GroupMemberMgr::GroupMemberMgr(GroupUsersDaoPtr groupUsersDao,
                                         IoCtxPool& ioCtxPool)
    : OfflineMemberMgrBase(groupUsersDao, ioCtxPool)
{
}

void GroupMemberMgr::handleUserEnterGroup(const std::string& uid,
                                               uint64_t gid)
{
    IoCtxPool::io_context_ptr ioc = m_ioCtxPool.getIoCtxByGid(gid);
    if (ioc != nullptr) {
        ioc->post([this, uid, gid]() {
            doHandleUserEnterGroup(uid, gid);
        });
    }
}

void GroupMemberMgr::handleUserLeaveGroup(const std::string& uid,
                                               uint64_t gid)
{
    IoCtxPool::io_context_ptr ioc = m_ioCtxPool.getIoCtxByGid(gid);
    if (ioc != nullptr) {
        ioc->post([this, uid, gid]() {
            doHandleUserLeaveGroup(uid, gid);
        });
    }
}

void GroupMemberMgr::handleUserMuteGroup(const std::string& uid,
                                              uint64_t gid)
{
    IoCtxPool::io_context_ptr ioc = m_ioCtxPool.getIoCtxByGid(gid);
    if (ioc != nullptr) {
        ioc->post([this, uid, gid]() {
            doHandleUserMuteGroup(uid, gid);
        });
    }
}

void GroupMemberMgr::handleUserUmuteGroup(const std::string& uid,
                                               uint64_t gid)
{
    IoCtxPool::io_context_ptr ioc = m_ioCtxPool.getIoCtxByGid(gid);
    if (ioc != nullptr) {
        ioc->post([this, uid, gid]() {
            doHandleUserUnmuteGroup(uid, gid);
        });
    }
}

void GroupMemberMgr::doHandleUserEnterGroup(const std::string& uid,
                                                 uint64_t gid)
{
    if (!isGroupExist(gid)) {
        return;
    }
    
    GroupUser user;
    dao::ErrorCode ec = m_groupUsersDao->getMember(gid, uid, user);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "get group member error: " << ec << ", gid: " << gid
             << ", uid: " << uid;
        return;
    }
    
    {
        std::unique_lock<std::shared_timed_mutex> l(m_memberMutex);
        if (user.role() == GroupUser::ROLE_SUBSCRIBER) {
            m_groupSubscribers[gid].insert(uid);
            removeGroupMemberNoLock(uid, gid);
        } else {
            m_groupMembers[gid].insert(uid);
            removeGroupSubscriberNoLock(uid, gid);
        }
    
        if ((user.status() & static_cast<int>(GroupStatus::MUTED)) != 0) {
            m_mutedMembers[gid].insert(uid);
        }
    }
}

void GroupMemberMgr::doHandleUserLeaveGroup(const std::string& uid,
                                                 uint64_t gid)
{
    if (!isGroupExist(gid)) {
        return;
    }
    
    if (!isMemberExists(uid, gid)) {
        LOGE << "member, uid: " << uid
             << " could not be found in group, gid: " << gid << ", todo reload";
        
        syncReloadGroupMembersFromDb(gid);
        return;
    }
    
    {
        std::unique_lock<std::shared_timed_mutex> l(m_memberMutex);
        removeGroupMemberNoLock(uid, gid);
        removeGroupSubscriberNoLock(uid, gid);
        removeMutedMemberNoLock(uid, gid);
    }
}

void GroupMemberMgr::doHandleUserMuteGroup(const std::string& uid,
                                                uint64_t gid)
{
    if (!isGroupExist(gid)) {
        return;
    }
    
    if (!isMemberExists(uid, gid)) {
        LOGE << "member, uid: " << uid
             << " could not be found in group, gid: " << gid << ", todo reload";
        
        syncReloadGroupMembersFromDb(gid);
        return;
    }
    
    {
        std::unique_lock<std::shared_timed_mutex> l(m_memberMutex);
        m_mutedMembers[gid].insert(uid);
    }
}

void GroupMemberMgr::doHandleUserUnmuteGroup(const std::string& uid,
                                                 uint64_t gid)
{
    if (!isGroupExist(gid)) {
        return;
    }
    
    if (isMemberExists(uid, gid)) {
        removeMutedMember(uid, gid);
    } else {
        LOGE << "member, uid: " << uid
             << " could not be found in group, gid: " << gid << ", todo reload";
    
        syncReloadGroupMembersFromDb(gid);
        return;
    }
}

void GroupMemberMgr::getUnmuteGroupMembers(uint64_t gid, UidSet& uids) const
{
    std::shared_lock<std::shared_timed_mutex> l(m_memberMutex);
    const auto it = m_groupMembers.find(gid);
    const auto itM = m_mutedMembers.find(gid);
    if (it != m_groupMembers.end()) {
        for (const auto& uid : it->second) {
            if ( (itM == m_mutedMembers.end()) ||
                 (itM->second.find(uid) == itM->second.end()) ) {
                uids.emplace(uid);
            }
        }
    }
}

void GroupMemberMgr::getUnmuteGroupSubscribers(uint64_t gid,
                                               UidSet& uids) const
{
    std::shared_lock<std::shared_timed_mutex> l(m_memberMutex);
    const auto it = m_groupSubscribers.find(gid);
    const auto itM = m_mutedMembers.find(gid);
    if (it != m_groupSubscribers.end()) {
        for (const auto& uid : it->second) {
            if ( (itM == m_mutedMembers.end()) ||
                 (itM->second.find(uid) == itM->second.end()) ) {
                uids.emplace(uid);
            }
        }
    }
}


bool GroupMemberMgr::loadGroupMembersFromDb(uint64_t gid)
{
    {
        std::shared_lock<std::shared_timed_mutex> l(m_memberMutex);
        if (m_groupMembers.find(gid) != m_groupMembers.end()) {
            return true;
        }

        if (m_groupSubscribers.find(gid) != m_groupSubscribers.end()) {
            return true;
        }
    }
    
    return syncReloadGroupMembersFromDb(gid);
}

bool GroupMemberMgr::syncReloadGroupMembersFromDb(uint64_t gid)
{
    {
        std::shared_lock<std::shared_timed_mutex> l(m_memberMutex);
        if (m_reloadTimes.find(gid) != m_reloadTimes.end()) {
            if ((nowInSec() - m_reloadTimes[gid]) < DB_RELOAD_GROUPUSER_INTERVAL) {
                return true;
            }
        }
        m_reloadTimes[gid] = nowInSec();
    }
    
    static const std::vector<GroupUser::Role> kRoleList = {
            GroupUser::ROLE_UNDEFINE,
            GroupUser::ROLE_OWNER,
            GroupUser::ROLE_ADMINISTROR,
            GroupUser::ROLE_MEMBER,
            GroupUser::ROLE_SUBSCRIBER
    };

    std::vector<bcm::GroupUser> members;
    dao::ErrorCode ec = m_groupUsersDao->getMemberRangeByRolesBatch(
            gid, kRoleList, members);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "get group member list error: " << ec << ", gid: " << gid;
        return false;
    }
    
    std::size_t numMuted = 0;
    std::size_t numMembers = 0;
    std::size_t numSubscribers = 0;
    {
        std::unique_lock<std::shared_timed_mutex> l1(m_memberMutex);
        m_groupMembers.erase(gid);
        m_groupSubscribers.erase(gid);
        m_mutedMembers.erase(gid);

        for (auto it = members.begin(); it != members.end(); ++it) {
            if (it->status() & GroupUser::STATUS_MUTED) {
                m_mutedMembers[gid].insert(it->uid());
                ++numMuted;
            }
            
            if (GroupUser::ROLE_SUBSCRIBER == it->role()) {
                m_groupSubscribers[gid].insert(it->uid());
                ++numSubscribers;
            } else {
                m_groupMembers[gid].insert(it->uid());
                ++numMembers;
            }
        }
    }
    
    LOGI << "load loadGroupMembersFromDb " << " members: " << numMembers
         << ", subscribers: " << numSubscribers
         << ", muted members: " << numMuted << ", for group " << gid;
    return true;
}


bool GroupMemberMgr::isMemberExists(const std::string& uid,
                                         uint64_t gid) const
{
    std::unique_lock<std::shared_timed_mutex> l(m_memberMutex);
    {
        auto it = m_groupMembers.find(gid);
        if (it != m_groupMembers.end()) {
            if (it->second.find(uid) != it->second.end()) {
                return true;
            }
        }
    }

    {
        auto it = m_groupSubscribers.find(gid);
        if (it != m_groupSubscribers.end()) {
            if (it->second.find(uid) != it->second.end()) {
                return true;
            }
        }
    }

    return false;
}

void GroupMemberMgr::removeMutedMember(const std::string& uid,
                                       uint64_t gid)
{
    std::unique_lock<std::shared_timed_mutex> l(m_memberMutex);
    removeMutedMemberNoLock(uid, gid);
}

void GroupMemberMgr::removeMutedMemberNoLock(const std::string& uid,
                                            uint64_t gid)
{
    auto it = m_mutedMembers.find(gid);
    if (it != m_mutedMembers.end()) {
        it->second.erase(uid);
        if (it->second.empty()) {
            m_mutedMembers.erase(gid);
        }
    }
}

} // namespace bcm

