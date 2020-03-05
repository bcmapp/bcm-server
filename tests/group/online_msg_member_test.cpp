#include "../test_common.h"

#include "group/group_msg_sub.h"
#include "utils/libevent_utils.h"
#include "redis/async_conn.h"
#include "redis/reply.h"

#include "../../src/group/io_ctx_pool.h"
#include "../../src/group/online_msg_member_mgr.h"
#include "../../src/dispatcher/dispatch_channel.h"
#include <thread>
#include <chrono>

#include <hiredis/hiredis.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <boost/core/ignore_unused.hpp>


using namespace bcm;
using namespace bcm::dao;

static std::map<uint64_t, std::set<std::string>> groupUsers = {
    {
        1, {
               "1PiMmSJHHdyUBtdJ4BWMeRb6sVJyq7Cxcu",
               "1BbCNCwXYZH6rWZJnL6N4n88UxeCccpKNV",
               "18UGLpxWEo8P54F1pRyuuQ2PtCk2zLbfSc"
           }
    },
    {
        2, {
               "1PiMmSJHHdyUBtdJ4BWMeRb6sVJyq7Cxcu",
               "1DPWzvDB2UyyBCpFZwTujYfmsgKkg2Wgnu"
           }
    },
    {
        3, {
               "12RQ4TsBGCFZcrmJjjmsfuwFKf7Gs2Eq2w"
           }
    }
};


class MockGroupUsersDao : public bcm::dao::GroupUsers
{
public:
    virtual bcm::dao::ErrorCode
    getJoinedGroupsList(const std::string& uid, std::vector<bcm::dao::UserGroupDetail>& groups) override
    {
        for (const auto& it : groupUsers) {
            if (it.second.find(uid) != it.second.end()) {
                bcm::dao::UserGroupDetail detail;
                detail.group.set_gid(it.first);
                detail.user.set_role(bcm::GroupUser_Role::GroupUser_Role_ROLE_MEMBER);
                groups.emplace_back(detail);
            }
        }

        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode
    getGroupDetailByGid(uint64_t gid, const std::string& uid, bcm::dao::UserGroupDetail& detail) override
    {
        boost::ignore_unused(gid, uid);
        detail.user.set_role(bcm::GroupUser_Role::GroupUser_Role_ROLE_MEMBER);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode insert(const bcm::GroupUser& user) override
    {
        boost::ignore_unused(user);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode insertBatch(const std::vector<bcm::GroupUser>& users) override
    {
        boost::ignore_unused(users);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode getMemberRole(uint64_t gid, const std::string& uid, bcm::GroupUser::Role& role) override
    {
        boost::ignore_unused(gid, uid, role);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode getMemberRoles(uint64_t gid, std::map<std::string, bcm::GroupUser::Role>& userRoles) override
    {
        boost::ignore_unused(gid, userRoles);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode delMember(uint64_t gid, const std::string& uid) override
    {
        boost::ignore_unused(gid, uid);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode delMemberBatch(uint64_t gid, const std::vector<std::string>& uids) override
    {
        boost::ignore_unused(gid, uids);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode getMemberBatch(uint64_t gid,
                                     const std::vector<std::string>& uids,
                                     std::vector<bcm::GroupUser>& users) override
    {
        boost::ignore_unused(gid, uids, users);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode getMemberRangeByRolesBatch(uint64_t gid, const std::vector<bcm::GroupUser::Role>& roles,
                                                 std::vector<bcm::GroupUser>& users) override
    {
        boost::ignore_unused(gid, roles, users);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode getMemberRangeByRolesBatchWithOffset(uint64_t gid,
                                                           const std::vector<bcm::GroupUser::Role>& roles,
                                                           const std::string& startUid, int count,
                                                           std::vector<bcm::GroupUser>& users) override
    {
        boost::ignore_unused(gid, roles, startUid, count, users);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode getJoinedGroups(const std::string& uid, std::vector<uint64_t>& gids) override
    {
        for (const auto& it : groupUsers) {
            if (it.second.find(uid) != it.second.end()) {
                gids.emplace_back(it.first);
            }
        }
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode getGroupDetailByGidBatch(const std::vector<uint64_t>& gids, const std::string& uid,
                                               std::vector<UserGroupEntry>& entries) override
    {
        boost::ignore_unused(gids, uid, entries);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode getGroupOwner(uint64_t gid, std::string& owner) override
    {
        boost::ignore_unused(gid, owner);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode getMember(uint64_t gid, const std::string& uid, bcm::GroupUser& user) override
    {
        boost::ignore_unused(gid, uid, user);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode queryGroupMemberInfoByGid(uint64_t gid, GroupCounter& counter) override
    {
        boost::ignore_unused(gid, counter);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode queryGroupMemberInfoByGid(uint64_t gid, GroupCounter& counter, const std::string& querier,
                                                bcm::GroupUser::Role& querierRole, const std::string& nextOwner,
                                                bcm::GroupUser::Role& nextOwnerRole) override
    {
        boost::ignore_unused(gid, counter, querier, querierRole, nextOwner, nextOwnerRole);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode update(uint64_t gid, const std::string& uid, const nlohmann::json& upData) override
    {
        boost::ignore_unused(gid, uid, upData);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual ErrorCode updateIfEmpty(uint64_t gid, const std::string& uid, const nlohmann::json& upData) override
    {
        boost::ignore_unused(gid, uid, upData);
        return ERRORCODE_SUCCESS;
    }

    virtual ErrorCode getMembersOrderByCreateTime(uint64_t gid,
                                                  const std::vector<bcm::GroupUser::Role>& roles,
                                                  const std::string& startUid,
                                                  int64_t createTime,
                                                  int count,
                                                  std::vector<bcm::GroupUser>& users) override
    {
        boost::ignore_unused(gid, roles, startUid, createTime, count, users);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
};

class MessageHandler : public bcm::GroupMsgSub::IMessageHandler {
    bcm::GroupMsgSub& m_sub;
    std::size_t m_msgIdx;

public:
    MessageHandler(bcm::GroupMsgSub& sub)
        : m_sub(sub), m_msgIdx(0)
    {
    }

    void handleMessage(const std::string& chan,
                       const std::string& msg) override
    {
        TLOG << "handleMessage chan: " << chan << ", msg: " << msg;
    }
};

void doCheckThd(bcm::OnlineMsgMemberMgr* pmgr, bcm::GroupMsgSub*)
{
    TLOG << "start thread ";
    sleep(1);
    OnlineMsgMemberMgr::UserSet tmpGroupUsers;
    pmgr->getGroupMembers(1, tmpGroupUsers);
    REQUIRE(tmpGroupUsers.size() == 3);
    tmpGroupUsers.clear();
    pmgr->getGroupMembers(3, tmpGroupUsers);
    REQUIRE(tmpGroupUsers.size() == 1);
    tmpGroupUsers.clear();
    pmgr->getGroupMembers(2, tmpGroupUsers);
    REQUIRE(tmpGroupUsers.size() == 2);

    std::string uid1 = "12RQ4TsBGCFZcrmJjjmsfuwFKf7Gs2Eq2w";
    pmgr->handleUserOffline(DispatchAddress(uid1, Device::MASTER_ID));
    std::string uid2 = "1PiMmSJHHdyUBtdJ4BWMeRb6sVJyq7Cxcu";
    pmgr->handleUserOffline(DispatchAddress(uid2, Device::MASTER_ID));

    sleep(1);
    tmpGroupUsers.clear();
    pmgr->getGroupMembers(1, tmpGroupUsers);
    REQUIRE(tmpGroupUsers.size() == 2);

    tmpGroupUsers.clear();
    pmgr->getGroupMembers(2, tmpGroupUsers);
    REQUIRE(tmpGroupUsers.size() == 1);

    tmpGroupUsers.clear();
    pmgr->getGroupMembers(3, tmpGroupUsers);
    REQUIRE(tmpGroupUsers.size() == 0);

    // pSub->shutdown([](int status) {
    //     boost::ignore_unused(status);
    //     TLOG << "group message subscriber is shutdown";
    // });
}

TEST_CASE("getOnlineMsgMember")
{
    bcm::OnlineMsgMemberMgr::GroupUsersDaoPtr pgud(new MockGroupUsersDao());
    bcm::IoCtxPool pool(5);

    evthread_use_pthreads();
    struct event_base* eb = event_base_new();

    // add onlineRedisManager instance
    bcm::GroupMsgSub sub;
    MessageHandler msgHandler(sub);
    sub.addMessageHandler(&msgHandler);

    bcm::OnlineMsgMemberMgr mgr(pgud,pool);
    mgr.addGroupMsgSubHandler(&sub);

    DispatcherConfig config;
    config.concurrency = 5;

    std::set<std::string>  uids;
    for (const auto& it : groupUsers) {
        for (const auto& uid : it.second) {
            uids.insert(uid);
        }
    }

    for (const auto&it1 : uids) {
        mgr.handleUserOnline(DispatchAddress(it1, Device::MASTER_ID));
    }

    std::thread ct(doCheckThd, &mgr, &sub);


    std::thread t([eb]() {
        int res = 0;
        try {
            res = event_base_dispatch(eb);
            sleep(1);
        } catch (std::exception& e) {
            TLOG << "Publisher exception  res: " << res << " what: " << e.what();
        }
        TLOG << "event loop thread exit with code: " << res;
    });

    ct.join();
    t.join();

    TLOG << "getOnlineMsgMember thread terminated";
    event_base_free(eb);
}
