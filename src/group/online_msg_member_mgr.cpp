#include <proto/dao/error_code.pb.h>
#include "online_msg_member_mgr.h"
#include "io_ctx_pool.h"
#include "dao/group_users.h"
#include "utils/libevent_utils.h"
#include "utils/log.h"
#include "dispatcher/dispatch_channel.h"

namespace bcm {

OnlineMsgMemberMgr::OnlineMsgMemberMgr(GroupUsersDaoPtr groupUsersDao,
                                       IoCtxPool& ioCtxPool)
    : MemberMgrBase(groupUsersDao, ioCtxPool)
    , m_groupSub(nullptr)
{
}

void OnlineMsgMemberMgr::handleUserOnline(const DispatchAddress& user)
{
    IoCtxPool::io_context_ptr ioc = m_ioCtxPool.getIoCtxByUid(user.getUid());
    if (ioc != nullptr) {
        ioc->post([this, user]() {
            doHandleUserOnline(user);
        });
    }
}

void OnlineMsgMemberMgr::handleUserOffline(const DispatchAddress& user)
{
    IoCtxPool::io_context_ptr ioc = m_ioCtxPool.getIoCtxByUid(user.getUid());
    if (ioc != nullptr) {
        ioc->post([this, user]() {
            doHandleUserOffline(user);
        });
    }
}

void OnlineMsgMemberMgr::handleUserEnterGroup(const std::string& uid, 
                                              uint64_t gid)
{
    IoCtxPool::io_context_ptr ioc = m_ioCtxPool.getIoCtxByGid(gid);
    if (ioc != nullptr) {
        ioc->post([this, uid, gid]() {
            doHandleUserEnterGroup(uid, gid);
        });
    }
}

void OnlineMsgMemberMgr::handleUserLeaveGroup(const std::string& uid, 
                                              uint64_t gid)
{
    IoCtxPool::io_context_ptr ioc = m_ioCtxPool.getIoCtxByGid(gid);
    if (ioc != nullptr) {
        ioc->post([this, uid, gid]() {
            doHandleUserLeaveGroup(uid, gid);
        });
    }
}

void OnlineMsgMemberMgr::doHandleUserOnline(const DispatchAddress& user)
{
    LOGT << "user : " << user << " is online";
    {
        std::unique_lock<std::shared_timed_mutex> l(m_onlineUsersMtx);

        auto users = m_onlineUsers.find(user.getUid());

        if (users == m_onlineUsers.end()) {
            m_onlineUsers[user.getUid()].insert(user);
        } else if (users->second.find(user) != users->second.end()) {
            return;
        } else {
            users->second.insert(user);
        }
    }

    std::vector<dao::UserGroupDetail> details;
    dao::ErrorCode ec = m_groupUsersDao->getJoinedGroupsList(user.getUid(), details);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "get joined groups error: " << ec << ": " << user;
        return;
    }

    std::vector<uint64_t>  subscribeChans;

    {
        std::unique_lock<std::shared_timed_mutex> l2(m_memberMutex);
        for (auto& d : details) {
            if (m_groupMembers.find(d.group.gid()) == m_groupMembers.end()) {

                subscribeChans.push_back(d.group.gid());
            }

            if (d.user.role() > GroupUser::ROLE_UNDEFINE &&
                d.user.role() < GroupUser::ROLE_SUBSCRIBER) {
                m_groupMembers[d.group.gid()].insert(user);
            }
        }
    }

    if (!subscribeChans.empty()) {
        // subscribe channel
        if (m_groupSub) {
            m_groupSub->subscribeGids(subscribeChans);
        }
    }

}

void OnlineMsgMemberMgr::doHandleUserOffline(const DispatchAddress& user)
{
    LOGT << "user, uid: " << user << " is offline";
    {
        std::unique_lock<std::shared_timed_mutex> l(m_onlineUsersMtx);

        auto users = m_onlineUsers.find(user.getUid());

        if (users != m_onlineUsers.end() && users->second.find(user) != users->second.end()) {
            users->second.erase(user);
        }
    }

    std::vector<uint64_t> userGids;
    dao::ErrorCode ec = m_groupUsersDao->getJoinedGroups(user.getUid(), userGids);

    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "get joined groups error: " << ec << ": " << user;
        return;
    }

    if (!userGids.empty()) {
        removeMemberForGroups(user, userGids);
    }

    std::vector<uint64_t> unsubscribeChans;
    for (auto& d : userGids) {
        if (!isGroupExist(d)) {
            unsubscribeChans.push_back(d);
        }
    }

    if (!unsubscribeChans.empty()) {
        // unsubscribe channel
        if (m_groupSub) {
            m_groupSub->unsubcribeGids(unsubscribeChans);
        }
    }

}

void OnlineMsgMemberMgr::doHandleUserEnterGroup(const std::string& uid, 
                                                uint64_t gid)
{
    auto onlineUsers = getOnlineUsers(uid);
    if (onlineUsers.empty()) {
        LOGT << "could not find user " << uid << " in online uid set";
        return;
    }

    dao::UserGroupDetail detail;
    dao::ErrorCode ec =
        m_groupUsersDao->getGroupDetailByGid(gid, uid, detail);

    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "get group detail error: " << ec << ", gid: " << gid 
                << ", uid: " << uid;
        return;
    }

    std::vector<uint64_t>  subscribeChans;

    if (detail.user.role() > GroupUser::ROLE_UNDEFINE && 
            detail.user.role() < GroupUser::ROLE_SUBSCRIBER) {
        std::unique_lock<std::shared_timed_mutex> l(m_memberMutex);

        if (m_groupMembers.find(gid) == m_groupMembers.end()) {
            subscribeChans.push_back(gid);
        }

        for (auto& it : onlineUsers) {
            m_groupMembers[gid].insert(it);
        }
    }

    if (subscribeChans.size() > 0) {
        // subscribe channel
        if (m_groupSub) {
            m_groupSub->subscribeGids(subscribeChans);
        }
    }
}

void OnlineMsgMemberMgr::doHandleUserLeaveGroup(const std::string& uid, 
                                                uint64_t gid)
{
    auto onlineUsers = getOnlineUsers(uid);
    if (onlineUsers.empty()) {
        LOGT << "could not find user " << uid << " in online uid set";
        return;
    }

    removeGroupMembers(onlineUsers, gid);

    if (!isGroupExist(gid)) {
        std::vector<uint64_t>  subscribeChans;
        subscribeChans.push_back(gid);
        // unsubscribe channel
        if (m_groupSub) {
            m_groupSub->unsubcribeGids(subscribeChans);
        }
    }

}


void OnlineMsgMemberMgr::addGroupMsgSubHandler(GroupMsgSub* handler)
{
    m_groupSub = handler;
}

void OnlineMsgMemberMgr::getOnlineUsers(const std::string& start,
                                        uint64_t excludedGid,
                                        uint64_t iosVer,
                                        uint64_t androidVer,
                                        size_t count,
                                        UserSet& result,
                                        std::string& lastNoiseUid,
                                        std::shared_ptr<DispatchManager> pDispatchMgr)
{
    static auto versionSupported = [iosVer, androidVer](const ClientVersion& target) -> bool {
        if (target.ostype() == ClientVersion::OSTYPE_IOS) {
            return target.bcmbuildcode() >= iosVer;
        } else if (target.ostype() == ClientVersion::OSTYPE_ANDROID) {
            return target.bcmbuildcode() >= androidVer;
        }
        return false;
    };
    std::string pos = start;
    UserSet excludedUids;
    getGroupMembers(excludedGid, excludedUids);
    bool bStartOver = false;

    // if we have started over and the pos >= start, that means we have traversed all the users
    while (!(bStartOver && pos >= start)) {
        UserSet users;
        // traverse m_onlineUsers in a batch to void too much race condition
        {
            std::shared_lock<std::shared_timed_mutex> l(m_onlineUsersMtx);
            if (m_onlineUsers.empty()) {
                result.clear();
                return;
            }
            UserMap::const_iterator itr = m_onlineUsers.upper_bound(pos);
            size_t b = count;
            while (itr != m_onlineUsers.end() && b > 0) {
                for (auto user = itr->second.begin(); user != itr->second.end(); ++user)
                {
                    if (excludedUids.find(*user) == excludedUids.end()) {
                        users.insert(*user);
                        b--;
                    }
                    if (b == 0) {
                        break;
                    }
                }
                pos = itr->first;
                itr++;
            }
        }
        if (users.empty()) {
            pos = "";
            bStartOver = true;
            continue;
        }
        UserSet candidateUsers;
        for (const auto user : users) {
            std::shared_ptr<IDispatcher> pd = pDispatchMgr->getDispatcher(user);
            DispatchChannel* pdc = dynamic_cast<DispatchChannel*>(pd.get());
            if (pdc == nullptr) {
                continue;
            }
            std::shared_ptr<WebsocketSession> ps = pdc->getSession();
            if (ps == nullptr) {
                continue;
            }
            const auto& account = boost::any_cast<Account>(ps->getAuthenticated(false));
            auto dev = AccountsManager::getAuthDevice(account);
            if (dev.has_value()
                    && dev.get().has_clientversion()
                    && versionSupported(dev.get().clientversion())) {
                // the device is valid, so add it to candidate list
                candidateUsers.insert(user);
            }
        }
        for (const auto& id : candidateUsers) {
            auto res = result.insert(id);
            if (!res.second) {
                // if the uid is already in result, that means we have already started over
                // and meeting a valid noise receiver we have picked
                return;
            }
            lastNoiseUid = id.getUid();
            if (result.size() >= count) {
                return;
            }
        }
    }
}

} // namespace bcm

