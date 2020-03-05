#include "group_msg_service.h"
#include "io_ctx_pool.h"
#include "im_server_mgr.h"
#include "group_msg_sub.h"
#include "group_event_sub.h"
#include "online_msg_member_mgr.h"

#include "online_msg_handler.h"

#include "redis/async_conn.h"
#include "utils/log.h"
#include "utils/libevent_utils.h"
#include "utils/thread_utils.h"
#include "dao/group_users.h"
#include "proto/dao/group_msg.pb.h"
#include "proto/group/message.pb.h"
#include "dispatcher/dispatch_manager.h"

#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/thread.h>
#include <boost/thread/barrier.hpp>

#include <nlohmann/json.hpp>
#include <thread>
#include <shared_mutex>

#include "redis/redis_manager.h"
#include "utils/time.h"

namespace bcm {

static const std::string groupMessageService = "bcm_gmessager";
static const std::string receivedInternalMessageFromRedis = 
    "received_internal_message_from_redis";
static const std::string receivedGroupMessageFromRedis = 
    "received_group_message_from_redis";

// -----------------------------------------------------------------------------
// Section: GroupServiceImpl
// -----------------------------------------------------------------------------
class GroupMsgServiceImpl 
    : public GroupMsgSub::IMessageHandler
    , public DispatchManager::IUserStatusListener {

    std::unique_ptr<std::thread> m_thread;
    struct event_base* m_eb;

    std::shared_ptr<DispatchManager> m_dispathMgr;
    std::shared_ptr<dao::GroupUsers> m_groupUsersDao;

    IoCtxPool m_ioCtxPool;
    ImServerMgr m_imSvrMgr;
    GroupMsgSub m_groupMsgSub;
    OnlineMsgMemberMgr m_onlineMsgMemberMgr;
    OnlineMsgHandler m_onlineMsgHandler;
    GroupEventSub m_groupEventSub;

public:
    GroupMsgServiceImpl(const RedisConfig redisCfg, 
                        std::shared_ptr<DispatchManager> dispatchMgr,
                        const NoiseConfig& noiseCfg)
        : m_eb(event_base_new())
        , m_dispathMgr(dispatchMgr)
        , m_groupUsersDao(dao::ClientFactory::groupUsers())
        , m_ioCtxPool(5)
        , m_imSvrMgr(m_eb, redisCfg)
        , m_groupMsgSub()
        , m_onlineMsgMemberMgr(m_groupUsersDao, m_ioCtxPool)
        , m_onlineMsgHandler(dispatchMgr, m_onlineMsgMemberMgr, noiseCfg)
        , m_groupEventSub(m_eb, redisCfg, m_onlineMsgMemberMgr)
    {
        dispatchMgr->registerUserStatusListener(this);

        m_groupMsgSub.addMessageHandler(this);
        m_onlineMsgMemberMgr.addGroupMsgSubHandler(&m_groupMsgSub);

        m_thread = std::make_unique<std::thread>(
            std::bind(&GroupMsgServiceImpl::eventLoop, this));
    }

    virtual ~GroupMsgServiceImpl()
    {
        m_dispathMgr->unregisterUserStatusListener(this);
        m_imSvrMgr.shutdown([](int status) {
            LOGI << "im server manager is shutdown with status: " << status;
        });
        m_groupEventSub.stop([](bool isOk) {
            boost::ignore_unused(isOk);
            LOGI << "group event handler stopped";
        });
        m_ioCtxPool.shutdown(true);
        m_thread->join();
        event_base_free(m_eb);
    }

    void addRegKey(const std::string& key)
    {
        m_imSvrMgr.addRegKey(key);
    }

    void onUserOnline(const DispatchAddress& user) override
    {
        m_onlineMsgMemberMgr.handleUserOnline(user);
    }

    void onUserOffline(const DispatchAddress& user) override
    {
        m_onlineMsgMemberMgr.handleUserOffline(user);
    }

    void getLocalOnlineGroupMembers(uint64_t gid, uint32_t count, OnlineMsgMemberMgr::UserList& users)
    {
        OnlineMsgMemberMgr::UserList tmpUsers;
        m_onlineMsgMemberMgr.getGroupMembers(gid, tmpUsers);
        for (auto it = tmpUsers.begin(); it != tmpUsers.end(); ++it) {
            if (count > 0 && users.size() >= count) {
                break;
            }

            // for now, only master device returned for key distribution
            if (it->getDeviceid() == Device::MASTER_ID)
            {
                users.push_back(std::move(*it));
            }
        }
    }

private:
    void eventLoop()
    {
        setCurrentThreadName("group.msgsvc");
        int retVal = event_base_dispatch(m_eb);
        LOGI << "event_base_dispatch returned with value: " << retVal;
    }

    void handleMessage(const std::string& chan, 
                       const std::string& msg) override
    {
        LOGI << "subscription message received, channel: " << chan 
             << ", message: " << msg;
        try {
            nlohmann::json msgObj = nlohmann::json::parse(msg);
            m_onlineMsgHandler.handleMessage(chan, msgObj);
        } catch (std::exception& e) {
            LOGE << "exception caught: " << e.what() 
                 << " when handle message: " << msg 
                 << ", from channel: " << chan;
        }
    }
};

// -----------------------------------------------------------------------------
// Section: GroupService
// -----------------------------------------------------------------------------
GroupMsgService::GroupMsgService(const RedisConfig redisCfg, 
                                 std::shared_ptr<DispatchManager> dispatchMgr,
                                 const NoiseConfig& noiseCfg)
    : m_pImpl(new GroupMsgServiceImpl(redisCfg, dispatchMgr, noiseCfg))
    , m_impl(*m_pImpl)
{
}

GroupMsgService::~GroupMsgService()
{
    if (nullptr != m_pImpl) {
        delete m_pImpl;
    }
}

void GroupMsgService::addRegKey(const std::string& key)
{
    m_impl.addRegKey(key);
}

void GroupMsgService::notifyUserOnline(const DispatchAddress& user)
{
    m_impl.onUserOnline(user);
}

void GroupMsgService::notifyUserOffline(const DispatchAddress& user)
{
    m_impl.onUserOffline(user);
}

void GroupMsgService::updateRedisdbOfflineInfo(uint64_t gid, uint64_t mid, GroupMultibroadMessageInfo& groupMultibroadInfo)
{
    bcm::PushPeopleType pushType = bcm::PushPeopleType::PUSHPEOPLETYPE_TO_ALL;
    if (!groupMultibroadInfo.members.empty()) {
        pushType = bcm::PushPeopleType::PUSHPEOPLETYPE_TO_DESIGNATED_PERSON;
        char multiField[50];
        snprintf(multiField, sizeof(multiField), "%020lu_%020lu_%02d", gid, mid, pushType);
        if (!RedisDbManager::Instance()->hset(gid, REDISDB_KEY_GROUP_MULTI_LIST_INFO, std::string(multiField), groupMultibroadInfo.to_string())) {
            LOGE << "failed to hset group info to redis 'group_multi_msg_list', gid: " << gid
                 << ", mid: " << mid << ", broadInfo: " << groupMultibroadInfo.to_string();
            return;
        }
    }

    char groupField[50];
    snprintf(groupField, sizeof(groupField), "%020lu_%020lu_%02d", gid, mid, pushType);
    if (!RedisDbManager::Instance()->zadd(gid, REDISDB_KEY_GROUP_MSG_INFO, std::string(groupField), nowInSec())) {
        LOGE << "failed to zadd group info to redis 'group_msg_list', gid: " << gid
             << ", mid: " << mid << ", from_uid: " << groupMultibroadInfo.from_uid;
        return;
    }
}


void GroupMsgService::getLocalOnlineGroupMembers(uint64_t gid, uint32_t count, OnlineMsgMemberMgr::UserList& users)
{
    m_impl.getLocalOnlineGroupMembers(gid, count, users);
}

} // namespace bcm
