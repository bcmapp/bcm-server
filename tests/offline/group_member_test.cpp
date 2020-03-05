#include "../test_common.h"

#include <thread>
#include <chrono>

#include <hiredis/hiredis.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <boost/core/ignore_unused.hpp>

#include <utils/jsonable.h>
#include <utils/time.h>
#include <utils/log.h>
#include "utils/libevent_utils.h"
#include "utils/thread_utils.h"

#include "redis/async_conn.h"
#include "redis/reply.h"

#include "proto/dao/group_user.pb.h"

#include "../../src/dao/group_users.h"
#include "../../src/group/io_ctx_pool.h"
#include "../../src/group/group_event.h"
#include "../../src/group/message_type.h"

#include "../../src/program/offline-server/group_partition_mgr.h"
#include "../../src/program/offline-server/group_member_mgr.h"
#include "../../src/program/offline-server/groupuser_event_sub.h"


using namespace bcm;
using namespace bcm::dao;

static std::map<uint64_t, std::map<std::string, uint32_t>> groupUsers = {
        {
                1, {
                        {"1PiMmSJHHdyUBtdJ4BWMeRb6sVJyq7Cxcu", 1},
                        {"1BbCNCwXYZH6rWZJnL6N4n88UxeCccpKNV", 2},
                        {"18UGLpxWEo8P54F1pRyuuQ2PtCk2zLbfSc", 4},
                        {"1DPWzvDB2UyyBCpFZwTujYfmsgKkg2Wgnu", 5}
                   }
        }
};


class MockGroupUsersDao : public bcm::dao::GroupUsers
{
public:
    virtual bcm::dao::ErrorCode
    getJoinedGroupsList(const std::string& uid, std::vector<bcm::dao::UserGroupDetail>& groups)
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
    getGroupDetailByGid(uint64_t gid, const std::string& uid, bcm::dao::UserGroupDetail& detail)
    {
        boost::ignore_unused(gid, uid);
        detail.user.set_role(bcm::GroupUser_Role::GroupUser_Role_ROLE_MEMBER);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode insert(const bcm::GroupUser& user)
    {
        boost::ignore_unused(user);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode insertBatch(const std::vector<bcm::GroupUser>& users)
    {
        boost::ignore_unused(users);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode getMemberRole(uint64_t gid, const std::string& uid, bcm::GroupUser::Role& role)
    {
        boost::ignore_unused(gid, uid, role);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode getMemberRoles(uint64_t gid, std::map<std::string, bcm::GroupUser::Role>& userRoles)
    {
        boost::ignore_unused(gid, userRoles);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode delMember(uint64_t gid, const std::string& uid)
    {
        boost::ignore_unused(gid, uid);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode delMemberBatch(uint64_t gid, const std::vector<std::string>& uids)
    {
        boost::ignore_unused(gid, uids);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode getMemberBatch(uint64_t gid,
                                               const std::vector<std::string>& uids,
                                               std::vector<bcm::GroupUser>& users)
    {
        boost::ignore_unused(gid, uids, users);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode getMemberRangeByRolesBatch(uint64_t gid, const std::vector<bcm::GroupUser::Role>& roles,
                                                           std::vector<bcm::GroupUser>& users)
    {
        boost::ignore_unused(roles);
    
        if (groupUsers.find(gid) == groupUsers.end()) {
            return bcm::dao::ERRORCODE_SUCCESS;
        }
        
        for (const auto& it : groupUsers[gid]) {

            bcm::GroupUser detail;
            detail.set_gid(gid);
            detail.set_uid(it.first);
            if (it.second == 5) {
                detail.set_role(bcm::GroupUser_Role::GroupUser_Role_ROLE_MEMBER);
                detail.set_status(1);
            } else {
                detail.set_role(static_cast< ::bcm::GroupUser_Role >(it.second));
            }
            users.emplace_back(detail);
        }
        
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode getMemberRangeByRolesBatchWithOffset(uint64_t gid,
                                                                     const std::vector<bcm::GroupUser::Role>& roles,
                                                                     const std::string& startUid, int count,
                                                                     std::vector<bcm::GroupUser>& users)
    {
        boost::ignore_unused(gid, roles, startUid, count, users);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode getJoinedGroups(const std::string& uid, std::vector<uint64_t>& gids)
    {
        for (const auto& it : groupUsers) {
            if (it.second.find(uid) != it.second.end()) {
                gids.emplace_back(it.first);
            }
        }
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode getGroupDetailByGidBatch(const std::vector<uint64_t>& gids, const std::string& uid,
                                                         std::vector<UserGroupEntry>& entries)
    {
        boost::ignore_unused(gids, uid, entries);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode getGroupOwner(uint64_t gid, std::string& owner)
    {
        boost::ignore_unused(gid, owner);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode getMember(uint64_t gid, const std::string& uid, bcm::GroupUser& user)
    {
        if (groupUsers.find(gid) != groupUsers.end()) {
            if (groupUsers[gid].find(uid) != groupUsers[gid].end()) {
                user.set_gid(gid);
                user.set_uid(uid);
                if (groupUsers[gid][uid]== 5) {
                    user.set_role(bcm::GroupUser_Role::GroupUser_Role_ROLE_MEMBER);
                    user.set_status(1);
                } else {
                    user.set_role(static_cast< ::bcm::GroupUser_Role >(groupUsers[gid][uid]));
                }
            }
        }
        
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode queryGroupMemberInfoByGid(uint64_t gid, GroupCounter& counter)
    {
        boost::ignore_unused(gid, counter);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode queryGroupMemberInfoByGid(uint64_t gid, GroupCounter& counter, const std::string& querier,
                                                          bcm::GroupUser::Role& querierRole, const std::string& nextOwner,
                                                          bcm::GroupUser::Role& nextOwnerRole)
    {
        boost::ignore_unused(gid, counter, querier, querierRole, nextOwner, nextOwnerRole);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode update(uint64_t gid, const std::string& uid, const nlohmann::json& upData)
    {
        boost::ignore_unused(gid, uid, upData);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode updateIfEmpty(uint64_t gid, const std::string& uid, const nlohmann::json& upData) override
    {
        boost::ignore_unused(gid, uid, upData);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual ErrorCode getMembersOrderByCreateTime(uint64_t gid,
                                                  const std::vector<bcm::GroupUser::Role>& roles,
                                                  const std::string& startUid,
                                                  int64_t createTime,
                                                  int count,
                                                  std::vector<bcm::GroupUser>& users)
    {
        boost::ignore_unused(gid, roles, startUid, createTime, count, users);
        return bcm::dao::ERRORCODE_SUCCESS;
    } 
};


class MessageHandler
        : public GroupUserEventSub::IMessageHandler{
    
    GroupMemberMgr* m_groupMemberMgr;
    
public:
    MessageHandler(GroupMemberMgr* groupMemberMgr) : m_groupMemberMgr(groupMemberMgr)
    {

    }
    
    virtual ~MessageHandler()
    {

    }

private:
    
    void handleMessage(const std::string& chan,
                       const std::string& msg) override
    {
        LOGI << "subscription message received, channel: " << chan
             << ", message: " << msg;
        try {
            nlohmann::json msgObj = nlohmann::json::parse(msg);
            GroupEvent evt;
            msgObj.get_to(evt);
            
            if (m_groupMemberMgr->isGroupExist(evt.gid)) {
                // todo
                handleEvent(evt.type, evt.uid, evt.gid);
            }
            
        } catch (std::exception& e) {
            LOGE << "exception caught: " << e.what()
                 << " when handle message: " << msg
                 << ", from channel: " << chan;
        }
    }
    
    void handleEvent(int type, const std::string& uid,
                     uint64_t gid)
    {
        LOGI << "handleEvent message received, type: " << type
             << ", uid: " << uid << ", gid: " << gid ;
        
        switch (type) {
            case INTERNAL_USER_ENTER_GROUP:
                m_groupMemberMgr->handleUserEnterGroup(uid, gid);
                break;
            case INTERNAL_USER_QUIT_GROUP:
                m_groupMemberMgr->handleUserLeaveGroup(uid, gid);
                break;
            case INTERNAL_USER_MUTE_GROUP:
                m_groupMemberMgr->handleUserMuteGroup(uid, gid);
                break;
            case INTERNAL_USER_UNMUTE_GROUP:
                m_groupMemberMgr->handleUserUmuteGroup(uid, gid);
                break;
        }
    }
};



static const std::string kEventChn = "user_dddddfffff";

class Publisher : public redis::AsyncConn::ISubscriptionHandler {
    struct event_base* m_eb;
    bcm::redis::AsyncConn m_conn;
    std::size_t m_msgIdx;
    std::thread m_eventThread;
    
    bool  m_isConnected;
    
public:
    Publisher(const std::string& host, int port = 6379,
              const std::string& password = "")
            : m_eb(event_base_new())
            , m_conn(m_eb, host, port, password)
            , m_msgIdx(0)
            , m_isConnected(false)
    {
        TLOG << "Publisher init";
        m_conn.setOnReconnectHandler(std::bind(&Publisher::onConnect, this,
                                                    std::placeholders::_1));
        m_conn.start(std::bind(&Publisher::onConnect, this, std::placeholders::_1));
    
        m_eventThread = std::thread([&] {
            int ret = event_base_dispatch(m_eb);
            LOGI << "event loop finish: " << ret;
        });
    }
    
    virtual ~Publisher()
    {
        m_conn.shutdown([](int status) {
            boost::ignore_unused(status);
            TLOG << "publisher shutdown";
        });
    
        event_base_loopbreak(m_eb);
        m_eventThread.join();
    
        event_base_free(m_eb);
        m_eb = nullptr;
    }
    
    void onConnect(int status)
    {
        boost::ignore_unused(status);
        TLOG << "redis Publisher connected";
    
        m_isConnected = true;
    }
    
    bool isConnected() {
        return m_isConnected;
    }
    
    void exec(const std::string& msg)
    {
        m_conn.exec([this](int res, const bcm::redis::Reply& reply) {
                        REQUIRE(REDIS_OK == res);
                        REQUIRE(reply.isInteger());
                        std::cout << "redis: " << reply.getInteger() << ", idx: " << m_msgIdx << std::endl;
                        
                    }, "PUBLISH %b %b", kEventChn.c_str(), kEventChn.size(),
                    msg.c_str(), msg.size());
    }

private:
    
    void onSubscribeConnected(int status)
    {
        boost::ignore_unused(status);
    }
    
    void onSubscribe(const std::string& chan)
    {
        LOGT << "subscribed to channel " << chan;
    }
    
    void onUnsubscribe(const std::string& chan)
    {
        LOGT << "unsubscribed from channel " << chan;
    }
    
    void onMessage(const std::string& chan,
                   const std::string& msg)
    {
        boost::ignore_unused(chan, msg);
    }
    
    void onError(int code)
    {
        LOGE << "redis error: " << code;
    }
    
    void onPublishConnected(int status)
    {
        boost::ignore_unused(status);
    }
    
};



static void doCheckThd(bcm::GroupMemberMgr* pmgr, bcm::GroupUserEventSub* pSub)
{
    TLOG << "start thread ";
    
    std::set<uint64_t>  gids;
    for (const auto& it : groupUsers) {
        gids.insert(it.first);
    }
    
    {
        for (const auto&it1 : gids) {
            pmgr->syncReloadGroupMembersFromDb(it1);
        }
    }
    
    sleep(1);
    
    std::set<std::string>  ss;
    pmgr->getUnmuteGroupMembers(1, ss);
    REQUIRE(ss.size() == 2);
    {
        for (const auto& it : ss) {
            if(it == "1PiMmSJHHdyUBtdJ4BWMeRb6sVJyq7Cxcu" || it == "1BbCNCwXYZH6rWZJnL6N4n88UxeCccpKNV" ) {
            
            } else {
                REQUIRE(it == "1PiMmSJHHdyUBtdJ4BWMeRb6sVJyq7Cxcu");
            }
        }
    }
    
    ss.clear();
    pmgr->getUnmuteGroupSubscribers(1, ss);
    REQUIRE(ss.size() == 1);
    {
        for (const auto& it : ss) {
            REQUIRE(it == "18UGLpxWEo8P54F1pRyuuQ2PtCk2zLbfSc");
        }
    }
    
    // test handleUserEnterGroup
    std::string uid1 = "dffffdddddfffffeeee";
    groupUsers[1][uid1] = 4;
    pmgr->handleUserEnterGroup(uid1, 1);
    
    std::string uid2 = "ffffeeeettrreeew";
    groupUsers[1][uid2] = 3;
    pmgr->handleUserEnterGroup(uid2, 1);
    
    std::string uid3 = "54365375648764874657";
    groupUsers[1][uid3] = 5;
    pmgr->handleUserEnterGroup(uid3, 1);
    
    std::string uid4 = "543653756487648746578";
    groupUsers[2][uid4] = 2;
    pmgr->handleUserEnterGroup(uid4, 2);
    
    sleep(1);
    ss.clear();
    pmgr->getUnmuteGroupMembers(1, ss);
    REQUIRE(ss.size() == 3);
    {
        for (const auto& it : ss) {
            if(it == "1PiMmSJHHdyUBtdJ4BWMeRb6sVJyq7Cxcu" || it == "1BbCNCwXYZH6rWZJnL6N4n88UxeCccpKNV" || it == uid2 ) {
            
            } else {
                REQUIRE(it == "1PiMmSJHHdyUBtdJ4BWMeRb6sVJyq7Cxcu");
            }
        }
    }
    
    ss.clear();
    pmgr->getUnmuteGroupSubscribers(1, ss);
    REQUIRE(ss.size() == 2);
    {
        for (const auto& it : ss) {
            if(it == "18UGLpxWEo8P54F1pRyuuQ2PtCk2zLbfSc" || it == uid1 ) {
        
            } else {
                REQUIRE(it == "18UGLpxWEo8P54F1pRyuuQ2PtCk2zLbfSc");
            }
        }
    }
    
    ss.clear();
    pmgr->getUnmuteGroupSubscribers(2, ss);
    REQUIRE(ss.size() == 0);

    // test  handleUserMuteGroup   handleUserUmuteGroup
    std::string uidUmute = "1DPWzvDB2UyyBCpFZwTujYfmsgKkg2Wgnu";
    pmgr->handleUserMuteGroup(uid2, 1);
    pmgr->handleUserUmuteGroup(uidUmute, 1);
    pmgr->handleUserUmuteGroup(uid3, 1);
    pmgr->handleUserMuteGroup(uid4, 2);
    pmgr->handleUserUmuteGroup(uid4, 2);
    
    
    sleep(1);
    ss.clear();
    pmgr->getUnmuteGroupMembers(1, ss);
    REQUIRE(ss.size() == 4);
    {
        for (const auto& it : ss) {
            if(it == "1PiMmSJHHdyUBtdJ4BWMeRb6sVJyq7Cxcu" || it == "1BbCNCwXYZH6rWZJnL6N4n88UxeCccpKNV" || it == uidUmute || it == uid3 ) {
            
            } else {
                REQUIRE(it == "1PiMmSJHHdyUBtdJ4BWMeRb6sVJyq7Cxcu");
            }
        }
    }
    
    ss.clear();
    pmgr->getUnmuteGroupSubscribers(1, ss);
    REQUIRE(ss.size() == 2);
    {
        for (const auto& it : ss) {
            if(it == "18UGLpxWEo8P54F1pRyuuQ2PtCk2zLbfSc" || it == uid1 ) {
            
            } else {
                REQUIRE(it == "18UGLpxWEo8P54F1pRyuuQ2PtCk2zLbfSc");
            }
        }
    }
    
    ss.clear();
    pmgr->getUnmuteGroupSubscribers(2, ss);
    REQUIRE(ss.size() == 0);
    
    // test handleUserLeaveGroup
    pmgr->handleUserLeaveGroup(uid1, 1);
    pmgr->handleUserLeaveGroup(uid2, 1);
    pmgr->handleUserLeaveGroup(uid3, 1);
    pmgr->handleUserLeaveGroup(uidUmute, 1);
    
    sleep(1);
    ss.clear();
    pmgr->getUnmuteGroupMembers(1, ss);
    REQUIRE(ss.size() == 2);
    {
        for (const auto& it : ss) {
            if(it == "1PiMmSJHHdyUBtdJ4BWMeRb6sVJyq7Cxcu" || it == "1BbCNCwXYZH6rWZJnL6N4n88UxeCccpKNV") {
            
            } else {
                REQUIRE(it == "1PiMmSJHHdyUBtdJ4BWMeRb6sVJyq7Cxcu");
            }
        }
    }
    
    ss.clear();
    pmgr->getUnmuteGroupSubscribers(1, ss);
    REQUIRE(ss.size() == 1);
    {
        for (const auto& it : ss) {
            if(it == "18UGLpxWEo8P54F1pRyuuQ2PtCk2zLbfSc") {
            
            } else {
                REQUIRE(it == "18UGLpxWEo8P54F1pRyuuQ2PtCk2zLbfSc");
            }
        }
    }
    
    
    // test publish
    {
        Publisher pu("127.0.0.1", 6379);
        sleep(1);

        std::string msg = "{\"type\" : 1, \"uid\" : \"dffffdddddfffffeeee\", \"gid\" : 1}";
        pu.exec(msg);
        msg = "{\"type\" : 1, \"uid\" : \"ffffeeeettrreeew\", \"gid\" : 1}";
        pu.exec(msg);
        msg = "{\"type\" : 2, \"uid\" : \"ffffeeeettrreeew\", \"gid\" : 1}";
        pu.exec(msg);
        msg = "{\"type\" : 1, \"uid\" : \"1DPWzvDB2UyyBCpFZwTujYfmsgKkg2Wgnu\", \"gid\" : 1}";
        pu.exec(msg);
        msg = "{\"type\" : 5, \"uid\" : \"1DPWzvDB2UyyBCpFZwTujYfmsgKkg2Wgnu\", \"gid\" : 1}";
        pu.exec(msg);
        sleep(1);
    }
    
    TLOG << "publisher end ";

    ss.clear();
    pmgr->getUnmuteGroupMembers(1, ss);
    {
        for (const auto& it : ss) {
            if (it == "1PiMmSJHHdyUBtdJ4BWMeRb6sVJyq7Cxcu" || it == "1BbCNCwXYZH6rWZJnL6N4n88UxeCccpKNV" || it == uidUmute) {
            
            } else {
                REQUIRE(it == "1PiMmSJHHdyUBtdJ4BWMeRb6sVJyq7Cxcu");
            }
            TLOG << "publisher end : " << it;
        }
    }
    REQUIRE(ss.size() == 3);
    
    ss.clear();
    pmgr->getUnmuteGroupSubscribers(1, ss);
    REQUIRE(ss.size() == 2);
    {
        for (const auto& it : ss) {
            if(it == "18UGLpxWEo8P54F1pRyuuQ2PtCk2zLbfSc" || it == uid1) {
            
            } else {
                REQUIRE(it == "18UGLpxWEo8P54F1pRyuuQ2PtCk2zLbfSc");
            }
        }
    }
    
    sleep(7);
    {
        for (const auto&it1 : gids) {
            pmgr->syncReloadGroupMembersFromDb(it1);
        }
    }
    
    pSub->shutdown([](int status) {
        boost::ignore_unused(status);
        TLOG << "group event subscriber is shutdown";
    });
    
    sleep(1);
}

TEST_CASE("offlineGroupMemberTest")
{
    std::shared_ptr<dao::GroupUsers> m_groupUsersDao(new MockGroupUsersDao());
    bcm::IoCtxPool pool(5);
    
    evthread_use_pthreads();
    
    struct event_base* eb = event_base_new();

    GroupMemberMgr m_groupMemberMgr(m_groupUsersDao, pool);

    GroupUserEventSub m_groupEventSub(eb, "127.0.0.1", 6379, "");
    
    MessageHandler msgHandler(&m_groupMemberMgr);
    m_groupEventSub.addMessageHandler(&msgHandler);
    
    
    std::set<uint64_t>  gids;
    for (const auto& it : groupUsers) {
        gids.insert(it.first);
    }
    
    for (const auto&it1 : gids) {
        m_groupMemberMgr.loadGroupMembersFromDb(it1);
    }
    
    std::thread ct(doCheckThd, &m_groupMemberMgr, &m_groupEventSub);
    
    
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
    
    TLOG << "offlineGroupMemberTest thread terminated";
    event_base_free(eb);
}
