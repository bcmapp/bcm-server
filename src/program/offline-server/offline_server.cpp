#include "offline_server.h"

#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/thread/barrier.hpp>
#include <fiber/asio_yield.h>

#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/thread.h>

#include <nlohmann/json.hpp>
#include <thread>
#include <shared_mutex>

#include <utils/jsonable.h>
#include <utils/time.h>
#include <utils/log.h>
#include "utils/libevent_utils.h"
#include "utils/thread_utils.h"

#include "../../group/io_ctx_pool.h"
#include "../../group/group_event.h"
#include "../../group/message_type.h"
#include "../../group/io_ctx_executor.h"

#include "../../config/group_store_format.h"


#include "../../proto/dao/group_msg.pb.h"
#include "../../proto/dao/account.pb.h"

#include "../../redis/async_conn.h"
#include "../../redis/hiredis_client.h"

#include "groupuser_event_sub.h"
#include "group_partition_mgr.h"
#include "group_member_mgr.h"
#include "http_post_request.h"

namespace bcm {

class OfflineServiceImpl
    : public GroupUserEventSub::IMessageHandler{

    struct event_base* m_eb;
    
    std::shared_ptr<dao::GroupUsers> m_groupUsersDao;
    std::shared_ptr<AccountsManager> m_accountMgr;
    
    IoCtxPool m_ioCtxPool;
    
    OfflineConfig m_config;
    
    GroupMemberMgr m_groupMemberMgr;
    
    GroupUserEventSub m_groupEventSub;
    
    IoCtxExecutor m_workThdPool;
    
    std::unique_ptr<std::thread> m_thread;
    
    std::shared_ptr<OfflineServiceRegister> m_offlineReg;
    
    std::map<int32_t /* redis index */ , RedisConfig> m_redisDbHosts;
    
    std::shared_ptr<push::Service> m_pushService;
    
    std::atomic<std::int64_t> m_runRoundTaskCount;
    std::atomic<std::uint64_t> m_runRoundStartTime;
    
    boost::asio::ssl::context m_sslCtx;
    
public:
    OfflineServiceImpl(const RedisConfig& redisCfg,
                       const OfflineConfig& offlineCfg,
                       std::shared_ptr<AccountsManager> accountMgr,
                       std::shared_ptr<OfflineServiceRegister> offlineReg,
                       std::map<int32_t, RedisConfig>& redisDbHosts,
                       std::shared_ptr<push::Service> pushService)
        : m_eb(event_base_new())
          , m_groupUsersDao(dao::ClientFactory::groupUsers())
          , m_accountMgr(accountMgr)
          , m_ioCtxPool(offlineCfg.offlineSvr.eventThreadNumb)
          , m_config(offlineCfg)
          , m_groupMemberMgr(m_groupUsersDao, m_ioCtxPool)
          , m_groupEventSub(m_eb, redisCfg.ip, redisCfg.port, redisCfg.password)
          , m_workThdPool(offlineCfg.offlineSvr.pushThreadNumb)
          , m_offlineReg(offlineReg)
          , m_redisDbHosts(redisDbHosts)
          , m_pushService(pushService)
          , m_runRoundTaskCount(0)
          , m_runRoundStartTime(0)
          , m_sslCtx(ssl::context::sslv23)
    {
        m_groupEventSub.addMessageHandler(this);
        
        m_thread = std::make_unique<std::thread>(
            std::bind(&OfflineServiceImpl::eventLoop, this));
    }

    virtual ~OfflineServiceImpl()
    {
        m_groupEventSub.shutdown([](int status) {
            LOGI << "group event subscription is shutdown with status: "
                 << status;
        });

        m_ioCtxPool.shutdown(true);
        m_thread->join();
        event_base_free(m_eb);
    }
    
    void  dbGetAndDeleteOneRedisMultiMsg(int32_t redisId, const std::vector<std::string>& vecMulitMsgs,
                                 std::map<uint64_t, GroupMessageInfoTask>& newGroupMsg)
    {
        std::map<std::string /* dbKey */, std::string> mapFieldValue;
        bool res = RedisClientSync::OfflineInstance()->hmget(redisId,
                                                  REDISDB_KEY_GROUP_MULTI_LIST_INFO,
                                                  vecMulitMsgs, mapFieldValue);
        if (!res) {
            LOGE << "redis hmget error, redisId: " << redisId
                    << ", key: " << REDISDB_KEY_GROUP_MULTI_LIST_INFO
                    << ", multi message size: " << vecMulitMsgs.size()
                    << ", multi message: " << toString(vecMulitMsgs);
            return;
        }
        
        for (const auto& itMsg : mapFieldValue) {
            std::vector<std::string> groupInfos;
            boost::split(groupInfos, itMsg.first, boost::is_any_of("_"));
    
            if (groupInfos.size() != 3) {
                LOGE << "redis group list format error redisId: " << redisId
                     << ", dbKey: " << itMsg.first << ", member: " << itMsg.second;
                continue;
            }
            
            uint64_t  curGid        = std::stoull(groupInfos[0]);
            uint64_t  curLastMid    = std::stoull(groupInfos[1]);
    
            auto itTask = newGroupMsg.find(curGid);
            if (itTask == newGroupMsg.end()) {
                LOGE << "redis group message id is not found, redisId: " << redisId
                     << ", dbKey: " << itMsg.first << ", member: " << itMsg.second;
                continue;
            }
            
            for (auto& itGm : itTask->second.gms) {
                if (itGm.last_mid != curLastMid) {
                    continue;
                }
                
                if (!itGm.gmm.from_string(itMsg.second)) {
                    LOGE << "redis group multi message member format error, redisId: " << redisId
                         << ", dbKey: " << itMsg.first << ", member: " << itMsg.second;
                    continue;
                }
    
                itTask->second.multicastMembers.insert(itGm.gmm.members.begin(), itGm.gmm.members.end());
            }
        }
        
        if (!vecMulitMsgs.empty()) {
            res = RedisClientSync::OfflineInstance()->hdel(redisId,
                                                           REDISDB_KEY_GROUP_MULTI_LIST_INFO, vecMulitMsgs);
        }
    }
    
    bool  dbGetAndDeleteOneRedisGroupMsg(int32_t redisId, std::map<uint64_t, GroupMessageInfoTask>& newGroupMsg)
    {
        int32_t  redisGroupIndex    = 0;
        int64_t  minTimeStamp       = 0;
        int64_t  maxTimeStamp       = nowInSec() - OFFLINE_GROUP_MESSAGE_DELAY_TIME;
        int32_t  recordOffset       = 0;
        int32_t  recordSize         = OFFLINE_GROUP_MESSAGE_SCAN_SIZE;
        
        bool resultRedisData = false;
        
        while (true) {
            std::vector<std::string>        mGetMultiMsgs;
            std::vector<ZSetMemberScore>    resultGroups;
            std::vector<std::string>        cleanGroupMsg;
            
            bool res = RedisClientSync::OfflineInstance()->getMemsByScoreWithLimit(redisId,
                                                                 REDISDB_KEY_GROUP_MSG_INFO,
                                                                 minTimeStamp, maxTimeStamp,
                                                                 recordOffset, recordSize, resultGroups);
            if (!res) {
                LOGE << "hscan redis group list, redis id: " << redisId
                     << ", key: " << REDISDB_KEY_GROUP_MSG_INFO
                     << ", recordOffset: " << recordOffset << ", count: " << recordSize
                     << ", result: " << res
                     << ", return size: " << resultGroups.size();
                break;
            }
            
            for (const auto& itGroup : resultGroups) {
                std::vector<std::string> groupInfos;
                boost::split(groupInfos, itGroup.member, boost::is_any_of("_"));
                
                if (groupInfos.size() != 3) {
                    cleanGroupMsg.emplace_back(itGroup.member);
                    LOGE << "redis group list format error redisId: " << redisId
                         << ", member: " << itGroup.member << ", tm: " << itGroup.score;
                    continue;
                }

                resultRedisData = true;
                
                uint64_t  curGid = std::stoull(groupInfos[0]);

                GroupMessageIdInfo gmDb;
                gmDb.last_mid   = std::stoull(groupInfos[1]);
                gmDb.type       = std::stol(groupInfos[2]);
                gmDb.tm         = itGroup.score;
                gmDb.dbKey      = itGroup.member;
                gmDb.redisId    = redisId;

                // check group message type
                if (gmDb.type != bcm::PushPeopleType::PUSHPEOPLETYPE_TO_ALL
                    && gmDb.type != bcm::PushPeopleType::PUSHPEOPLETYPE_TO_DESIGNATED_PERSON) {
    
                    LOGE << "redis group type error, redisId: " << redisId
                         << ", member: " << itGroup.member << ", tm: " << itGroup.score;
                    cleanGroupMsg.emplace_back(itGroup.member);
                    continue;
                }

                //  More than 30 minutes to discard
                if ( (nowInSec() - gmDb.tm) > OFFLINE_GROUP_MESSAGE_EXPIRE_TIME) {
                    cleanGroupMsg.emplace_back(itGroup.member);
                    continue;
                }

                GroupMessageInfoTask  tmpNew;
                // append Previous round messageId, timestamp
                GroupMessageSeqInfo  gmS;
                if (GroupPartitionMgr::Instance().getGroupPushInfo(curGid, gmS)) {
                    tmpNew.preRoundMid     = gmS.lastMid;
                    tmpNew.preRoundMsgTs   = gmS.timestamp;
                    
                    if (gmS.lastMid > gmDb.last_mid) {
                        cleanGroupMsg.emplace_back(itGroup.member);
                        LOGE << "redis group expire message, redisId: " << redisId
                                << ", member: " << itGroup.member << ", tm: " << itGroup.score
                             << ", current message tm: " << gmS.lastMid;
                        continue;
                    }
                }

                redisGroupIndex++;
                
                if (newGroupMsg.find(curGid) == newGroupMsg.end()) {
                    newGroupMsg[curGid] = tmpNew;
                }
                
                if (gmDb.type == bcm::PushPeopleType::PUSHPEOPLETYPE_TO_DESIGNATED_PERSON) {
                    newGroupMsg[curGid].multicastCount++;
                    mGetMultiMsgs.emplace_back(itGroup.member);
                } else {
                    newGroupMsg[curGid].broadcastCount++;
                }
                
                newGroupMsg[curGid].gms.emplace_back(gmDb);
                cleanGroupMsg.emplace_back(itGroup.member);
            }

            LOGI << "redis group list, redis id: " << redisId
                 << ", key: " << REDISDB_KEY_GROUP_MSG_INFO
                 << ", recordOffset: " << recordOffset << ", count: " << recordSize
                 << ", result: " << res
                 << ", return size: " << resultGroups.size();

            // clean redis key
            if (!cleanGroupMsg.empty()) {
                if(!RedisClientSync::OfflineInstance()->zrem(redisId,
                                                         REDISDB_KEY_GROUP_MSG_INFO, cleanGroupMsg)) {
                    break;
                }
            }
    
            // get multi message from redis
            if (!mGetMultiMsgs.empty()) {
                dbGetAndDeleteOneRedisMultiMsg(redisId, mGetMultiMsgs, newGroupMsg);
            }
            
            if ((int32_t)resultGroups.size() < recordSize) {
                break;
            }
        } // end while(true)
    
        LOGI << "updateOneRedis group list, redis id: " << redisId
                << ", one redis new group message count: " << redisGroupIndex
                << ", new message group count: " << newGroupMsg.size();
    
        return resultRedisData;
    }
    
    bool dbScanGroupUserMsgList(const std::vector<uint32_t>& vecDbList,
                                 const uint64_t gid,
                                 std::map<std::string /* uid */, GroupUserMessageIdInfo>& groupUserMsgId)
    {
        for (const auto& itRedis : vecDbList) {
            std::string cursor = "0";
            std::string new_cursor = "";
            int32_t  redisGroupUserIndex = 0;
        
            do {
                new_cursor = "";
                std::map<std::string /* uid */ , std::string>  resultGroupUsers;
            
                bool res = RedisClientSync::OfflineInstance()->hscan(itRedis,
                                                                     REDISDB_KEY_PREFIX_GROUP_USER_INFO + std::to_string(gid),
                                                                     cursor, "", OFFLINE_GROUP_USER_SCAN_SIZE,
                                                                     new_cursor, resultGroupUsers);
                if (!res) {
                    LOGW << "redis group_user list get error, redisId: " << itRedis
                         << ", gid: " << gid
                         << ", key: " << REDISDB_KEY_PREFIX_GROUP_USER_INFO << std::to_string(gid)
                         << ", cursor: " << cursor << ", count: " << OFFLINE_GROUP_USER_SCAN_SIZE
                         << ", new_cursor: " << new_cursor << ", result: " << res
                         << ", return size: " << resultGroupUsers.size();
                    break;
                }
                
                for (const auto& itUid : resultGroupUsers) {
                    GroupUserMessageIdInfo guDb;
                    if (!guDb.from_string(itUid.second)) {
                        LOGE << "redis group_user list format error, redisId: " << itRedis
                             << ", gid: " << gid
                             << ", uid: " << itUid.first
                             << ", msg: " << itUid.second;
                        continue;
                    }
                
                    redisGroupUserIndex++;
    
                    auto itUser = groupUserMsgId.find(itUid.first);
                    if (itUser != groupUserMsgId.end()) {
                        if (itUser->second.last_mid >= guDb.last_mid) {
                            continue;
                        }
                        itUser->second = guDb;
                    } else {
                        groupUserMsgId[itUid.first] = guDb;
                    }
                }
            
                LOGI << "redis group_user list, redisId: " << itRedis
                     << ", gid: " << gid
                     << ", key: " << REDISDB_KEY_PREFIX_GROUP_USER_INFO + std::to_string(gid)
                     << ", cursor: " << cursor << ", count: " << OFFLINE_GROUP_USER_SCAN_SIZE
                     << ", new_cursor: " << new_cursor << ", result: " << res
                     << ", return size: " << resultGroupUsers.size();
            
                // update redis cursor
                cursor = new_cursor;
            
            } while ("0" != new_cursor);
        
            LOGI << "redis group_user one redis list, redisId: " << itRedis
                 << ", gid: " << gid
                 << ", key: " << REDISDB_KEY_PREFIX_GROUP_USER_INFO + std::to_string(gid)
                 << ", group user size: " << redisGroupUserIndex;
        
        } // end for (const auto& itRedis : vecDbList)
    
        return true;
    }
    
    bool dbHmgetRedisGroupUserMsg(const std::vector<uint32_t>& vecDbList,
                               const uint64_t gid,
                               const std::vector<std::string>& memberUids,
                               std::map<std::string /* uid */, GroupUserMessageIdInfo>& groupUserMsgId)
    {
        for (const auto& itRedis : vecDbList) {
            std::map<std::string /* uid */ , std::string>  resultGroupUsers;
            
            bool res = RedisClientSync::OfflineInstance()->hmget(itRedis,
                                                                 REDISDB_KEY_PREFIX_GROUP_USER_INFO + std::to_string(gid),
                                                                 memberUids, resultGroupUsers);
            if (!res) {
                LOGW << "redis group_user list get error, redisId: " << itRedis
                     << ", gid: " << gid
                     << ", key: " << REDISDB_KEY_PREFIX_GROUP_USER_INFO + std::to_string(gid)
                     << ", member size: " << memberUids.size() << ", result: " << res
                     << ", return size: " << resultGroupUsers.size();
                continue;
            }
            
            for (const auto& itUid : resultGroupUsers) {
                GroupUserMessageIdInfo guDb;
                if (!guDb.from_string(itUid.second)) {
                    LOGE << "redis group_user list format error, redisId: " << itRedis
                         << ", gid: " << gid
                         << ", uid: " << itUid.first
                         << ", msg: " << itUid.second;
                    continue;
                }
                
                auto itUser = groupUserMsgId.find(itUid.first);
                if (itUser != groupUserMsgId.end()) {
                    if (itUser->second.last_mid >= guDb.last_mid) {
                        continue;
                    }
                    itUser->second = guDb;
                } else {
                    groupUserMsgId[itUid.first] = guDb;
                }
            }
    
            LOGI << "redis group_user list, redisId: " << itRedis
                 << ", gid: " << gid
                 << ", key: " << REDISDB_KEY_PREFIX_GROUP_USER_INFO << std::to_string(gid)
                 << ", member size: " << memberUids.size() << ", result: " << res
                 << ", return size: " << resultGroupUsers.size();
            
        } // end for (const auto& itRedis : vecDbList)
    
        return true;
    }

    bool dbGetAccountsPushType(const std::vector<std::string>& uids,
                               std::map<std::string, GroupUserMessageIdInfo>& groupUserMsgId,
                               std::vector<std::string>& missedUids)
    {
        std::vector<Account> accountList;
        std::vector<std::string> m_missedUids;
        
        if (!m_accountMgr->get(uids, accountList, m_missedUids)) {
            LOGE <<"dbGetAccountsPushType get account database error"
                 << ", uids: " << toString(uids);
            return false;
        }
    
        if (!m_missedUids.empty()) {
            LOGW << m_missedUids.size() << "uids could not be found in database, "
                                         << "missed uids: " << toString(m_missedUids);
            
            for (const auto& itAcc : m_missedUids) {
                groupUserMsgId[itAcc].cfgFlag = GroupUserConfigPushType::NO_CONFIG;
                missedUids.emplace_back(itAcc);
            }
        }
    
        for (const auto& a : accountList) {
            const auto& dev = AccountsManager::getDevice(a, Device::MASTER_ID);
            if (dev == boost::none) {
                LOGW << "account, uid: " << a.uid() << " does not have a master device";
                continue;
            }
        
            if (!AccountsManager::isDevicePushable(*dev)) {
                groupUserMsgId[a.uid()].cfgFlag = GroupUserConfigPushType::NO_CONFIG;
                continue;
            }
            
            auto itgum = groupUserMsgId.find(a.uid());
            if (itgum != groupUserMsgId.end()) {
                itgum->second.cfgFlag = GroupUserConfigPushType::NORMAL;
                itgum->second.gcmId = dev->gcmid();
                itgum->second.umengId = dev->umengid();
                itgum->second.osType = dev->clientversion().ostype();
                itgum->second.osVersion = dev->clientversion().osversion();
                itgum->second.bcmBuildCode = dev->clientversion().bcmbuildcode();
                itgum->second.phoneModel = dev->clientversion().phonemodel();
                itgum->second.targetAddress = DispatchAddress(a.uid(), dev->id()).getSerialized();
                itgum->second.apnId = dev->apnid();
                itgum->second.apnType = dev->apntype();
                itgum->second.voipApnId = dev->voipapnid();
            }
        }
        return true;
    }
    
    bool dbBatchGetAccountsPushType(const std::vector<std::string>& uids,
                                    std::map<std::string, GroupUserMessageIdInfo>& groupUserMsgId,
                                    std::vector<std::string>& missedUids)
    {
        std::vector<std::string>   partUids;
        
        int32_t numb = 0;
        for (const auto& itUid : uids) {
            partUids.emplace_back(itUid);

            numb++;
            if (numb >= 20) {
                numb    = 0;
    
                if (!dbGetAccountsPushType(partUids, groupUserMsgId, missedUids)) {
                    return false;
                }
                partUids.clear();
            }
        }
        
        if (numb > 0) {
            if (!dbGetAccountsPushType(partUids, groupUserMsgId, missedUids)) {
                return false;
            }
        }
        return true;
    }
    
    void doPostGroupMessage(const std::string& offlineSvrAddr,
                          uint64_t gid, uint64_t mid,
                          const std::map<std::string, std::string>& destinations)
    {
        nlohmann::json j = {
                {"gid", std::to_string(gid)},
                {"mid", std::to_string(mid)},
                {"destinations", destinations}
        };
    
        LOGI << "host: " << offlineSvrAddr << "gid: " << gid << ", mid: " << mid << ", uids: " << toString(destinations);
        
        HttpPostRequest::shared_ptr req =
                std::make_shared<HttpPostRequest>(*(m_workThdPool.io_context()), m_sslCtx, kOfflinePushMessageUrl);
        req->setServerAddr(offlineSvrAddr)->setPostData(j.dump())->exec();
    }
    
    void handleMemberUpdateMessage(const std::vector<uint32_t>& vecDbList, const uint64_t gid,
                                   const GroupMessageIdInfo& gm,
                                   std::map<std::string /* uid */, GroupUserMessageIdInfo>&  groupUserMsgId)
    {
        LOGI << "handleMemberUpdateMessage push member update message"
             << ", gid: " << gid << ", mid: " << gm.last_mid << ", uids: " << toString(gm.gmm.members);
        
        std::map<std::string /* uid */, GroupUserMessageIdInfo>  offlineUids;
        if (!getOfflineAccountAndPushType(gm.gmm.members, gid, gm, groupUserMsgId, offlineUids)) {
            return;
        }
        
        auto itSender = offlineUids.find(gm.gmm.from_uid);
        if (itSender != offlineUids.end()) {
            offlineUids.erase(itSender);
        }
        
        if (offlineUids.empty()) {
            LOGI << "all members of group are currently online, gid: " << gid
                 << ", mid: " << gm.last_mid << ", time: " << gm.tm
                 << ", db_key: " << gm.dbKey << ", members: " << toString(gm.gmm.members);
            return;
        }
        
        // push message
        doPushMessage(vecDbList, gid, gm, offlineUids);
    }
    
    void handleGroupMessagePush(const std::vector<uint32_t>& vecDbList, const uint64_t gid,
                                const GroupMessageIdInfo& gm, std::set<std::string>& memberUids,
                                std::map<std::string /* uid */, GroupUserMessageIdInfo>&  groupUserMsgId)
    {
        // check offline uid
        std::map<std::string /* uid */, GroupUserMessageIdInfo>  offlineUids;
        if (!getOfflineAccountAndPushType(memberUids, gid, gm, groupUserMsgId, offlineUids)) {
            return;
        }
    
        // push message
        doPushMessage(vecDbList, gid, gm, offlineUids);
        
    }
    
    void doPushMessage(const std::vector<uint32_t>& vecDbList, const uint64_t gid,
                       const GroupMessageIdInfo& gm,
                       std::map<std::string /* uid */, GroupUserMessageIdInfo>&  offlineUids)
    {
        std::vector<HField>  vecHashDb;
        std::map<std::string /* pushType */, std::map<std::string /* uid */, std::string> >  mPushOther;
        
        for (auto& itPush : offlineUids) {
            auto& gumi = itPush.second;
            gumi.last_mid = gm.last_mid;

            push::Notification notification;
            notification.group(std::to_string(gid), std::to_string(gumi.last_mid));
            notification.setApnsType(gumi.apnType);
            notification.setApnsId(gumi.apnId);
            notification.setVoipApnsId(gumi.voipApnId);
            notification.setFcmId(gumi.gcmId);
            notification.setUmengId(gumi.umengId);

            ClientVersion cv;
            cv.set_ostype(static_cast<ClientVersion::OSType>(gumi.osType));
            cv.set_osversion(gumi.osVersion);
            cv.set_bcmbuildcode(gumi.bcmBuildCode);
            cv.set_phonemodel(gumi.phoneModel);
            notification.setClientVersion(std::move(cv));

            auto address = DispatchAddress::deserialize(gumi.targetAddress);
            if (address != boost::none) {
                notification.setTargetAddress(*address);
            }


            std::string  pushType = notification.getPushType();
            if (m_config.offlineSvr.checkPushType(pushType)) {
                if (m_config.offlineSvr.isPush) {
                    m_pushService->sendNotification(pushType, notification);
                }
            } else {
                // post to other offline server
                if (!pushType.empty()) {
                    mPushOther[pushType][itPush.first] = itPush.second.to_string();
                }
            }
    
            HField item(itPush.first, itPush.second.to_string());
            vecHashDb.emplace_back(item);
        }
        
        for (const auto& itOther : mPushOther) {
            std::string offSvrAddr = m_offlineReg->getRandomOfflineServerByType(itOther.first);
            
            if ("" != offSvrAddr && m_config.offlineSvr.isPush) {
                doPostGroupMessage(offSvrAddr, gid, gm.last_mid, itOther.second);
            } else {
                LOGE << "getRandomOfflineServerByType false, gid: " << gid
                     << ", mid: " << gm.last_mid
                     << ", pushType: " << itOther.first
                     << ", offSvrAddr: " << offSvrAddr
                     << ", isPush: " << m_config.offlineSvr.isPush
                     << ", desc: " << toString(itOther.second);
            }
        }
        
        for (const auto& itRedis : vecDbList) {
            std::unordered_map <std::string /* uid */ , std::string> resultGroupUsers;
        
            bool res = RedisClientSync::OfflineInstance()->hmset(itRedis,
                                                                 REDISDB_KEY_PREFIX_GROUP_USER_INFO + std::to_string(gid),
                                                                 vecHashDb);
            if (res) {
                break;
            }
        }
    }
    
    void handleOfflineGroupMessage(const std::vector<uint32_t>& vecDbList, const uint64_t gid, const GroupMessageInfoTask& gmt)
    {
        if (!m_groupMemberMgr.loadGroupMembersFromDb(gid)) {
            return;
        }
    
        std::set<std::string> memberUids;
        m_groupMemberMgr.getUnmuteGroupMembers(gid, memberUids);
        if (memberUids.empty()) {
            LOGI << "group, gid: " << gid << " has no member";
            return;
        }
    
        LOGI << "group, gid: " << gid << ", members: " << memberUids.size()
             << ", msg broadcast size: " << gmt.broadcastCount
             << ", msg multicast size: " << gmt.multicastCount
             << ", preRoundMid: " << gmt.preRoundMid
             << ", msg count: " << gmt.gms.size();
    
        // get redisDb group user info
        std::map<std::string /* uid */, GroupUserMessageIdInfo>  m_groupUserMsgId;
        if (gmt.broadcastCount > 0) {
            dbScanGroupUserMsgList(vecDbList, gid, m_groupUserMsgId);
        } else {
            std::vector<std::string>  vecMembers(gmt.multicastMembers.begin(), gmt.multicastMembers.end());
            dbHmgetRedisGroupUserMsg(vecDbList, gid, vecMembers, m_groupUserMsgId);
        }
        
        // start process group message
        for (const auto& itGroupMsg : gmt.gms) {
            if (itGroupMsg.type == bcm::PushPeopleType::PUSHPEOPLETYPE_TO_DESIGNATED_PERSON) {
                handleMemberUpdateMessage(vecDbList, gid, itGroupMsg, m_groupUserMsgId);
            } else if (itGroupMsg.type == bcm::PushPeopleType::PUSHPEOPLETYPE_TO_ALL) {
                handleGroupMessagePush(vecDbList, gid, itGroupMsg, memberUids, m_groupUserMsgId);
            }
    
            // update GroupPartitionMgr
            GroupPartitionMgr::Instance().updateMid(gid, itGroupMsg.tm, itGroupMsg.last_mid);
        }
        
        // todo check and clear group member
        std::map<std::string, bcm::GroupUser::Role> groupMissMap;
        for (const auto& itGu : m_groupUserMsgId) {
            if (memberUids.find(itGu.first) == memberUids.end()) {
                groupMissMap[itGu.first]    = GroupUser::ROLE_UNDEFINE;
            }
        }
    
        if (!groupMissMap.empty()) {
            auto roleMapRet = m_groupUsersDao->getMemberRoles(gid, groupMissMap);
            
            if (roleMapRet == dao::ERRORCODE_SUCCESS || roleMapRet == dao::ERRORCODE_NO_SUCH_DATA) {
                bool isReloadGroupMember  = false;
                std::vector<std::string> missedUids;
                for (const auto& itDaoUid : groupMissMap) {
                    if (itDaoUid.second != GroupUser::ROLE_UNDEFINE) {
                        if (!m_groupMemberMgr.isMemberExists(itDaoUid.first, gid)) {
                            isReloadGroupMember = true;
                        }
                    } else {
                        missedUids.emplace_back(itDaoUid.first);
                    }
                }
    
                for (const auto& itRedis : vecDbList) {
                    RedisClientSync::OfflineInstance()->hdel(itRedis,
                                                             REDISDB_KEY_PREFIX_GROUP_USER_INFO + std::to_string(gid),
                                                             missedUids);
                }
    
                if (isReloadGroupMember) {
                    m_groupMemberMgr.syncReloadGroupMembersFromDb(gid);
                }
    
                LOGI << "delete group member , gid: " << gid << ", isReload group member: " << isReloadGroupMember
                     << ", missUid: " << toString(missedUids);
            } else {
                LOGE << "group dao getMemberRoles error, gid: " << gid << ", result: " << roleMapRet
                     << ", groupMissMap: " << toString(groupMissMap);
            }
        } // end if (!groupMissMap.empty())
    }
    
    bool getOfflineAccountAndPushType(const std::set<std::string>& groupMembers, uint64_t gid,
                                 const GroupMessageIdInfo& gm,
                                 std::map<std::string /* uid */, GroupUserMessageIdInfo>&  groupUserMsgId,
                                 std::map<std::string /* uid */, GroupUserMessageIdInfo>&  offlineUids)
    {
        std::vector<std::string> noPushTypeUids;            // no push type user list
        
        for (const auto& itUid : groupMembers) {
            auto itgum = groupUserMsgId.find(itUid);
            if (itgum != groupUserMsgId.end()) {
                if (itgum->second.last_mid >= gm.last_mid) {
                    continue;
                }
                
                if (itgum->second.cfgFlag == GroupUserConfigPushType::NO_CONFIG) {
                    LOGW << "user not config push, gid: " << gid
                         << ", uid: " << itUid
                         << ", mid: " << gm.last_mid
                         << ", gcm: " << itgum->second.gcmId
                         << ", umengId: " << itgum->second.umengId
                         << ", apnId: " << itgum->second.apnId;
                    continue;
                }
                
                offlineUids[itUid] = itgum->second;
                
                if ( "" == itgum->second.gcmId
                     && "" == itgum->second.umengId
                     && "" == itgum->second.apnId) {
                    noPushTypeUids.emplace_back(itUid);
                } else {
                    // append new osVersion  20191017
                    if ("" != itgum->second.apnId && "" == itgum->second.osVersion) {
                        noPushTypeUids.emplace_back(itUid);
                    }
                }
            } else {
                GroupUserMessageIdInfo  tmp;
                tmp.last_mid = gm.last_mid;
                offlineUids[itUid] = tmp;
                
                noPushTypeUids.emplace_back(itUid);
            }
        }
        
        if (offlineUids.empty()) {
            LOGI << "all members of group are currently online, gid: " << gid
                 << ", mid: " << gm.last_mid << ", time: " << gm.tm << ", db_key: " << gm.dbKey;
            return false;
        }
        
        // get accounts info
        std::vector<std::string> missedUids;
        if (!dbBatchGetAccountsPushType(noPushTypeUids, offlineUids, missedUids)) {
            return false;
        }
        
        // delete missed group user
        if (!missedUids.empty()) {
            RedisClientSync::OfflineInstance()->hdel(gm.redisId,
                                                     REDISDB_KEY_PREFIX_GROUP_USER_INFO + std::to_string(gid),
                                                     missedUids);
        }
        
        // update cache
        for (const auto& itPush : noPushTypeUids) {
            auto itOffline = offlineUids.find(itPush);
            if (itOffline != offlineUids.end()) {
                groupUserMsgId[itPush] = itOffline->second;
            }
        }
    
        return true;
    }
    
    // start round offline server
    void runRoundOfflinePush()
    {
        // get redis group list
        std::map<uint64_t, GroupMessageInfoTask> newGroupMsgSeq;
        std::vector<uint32_t>    vecDbLists;
        
        // check redisDb at new group message
        for (const auto& itDb : m_redisDbHosts) {
            std::string value = "";
            if (!RedisClientSync::OfflineInstance()->get(itDb.first, REDISDB_KEY_GROUP_REDIS_ACTIVE, value)) {
                continue;
            }
    
            if ("" != value) {
                if (dbGetAndDeleteOneRedisGroupMsg(itDb.first, newGroupMsgSeq)) {
                    vecDbLists.emplace_back(itDb.first);
                }
            }
        }
    
        m_runRoundTaskCount.store(newGroupMsgSeq.size(), std::memory_order_seq_cst);
        m_runRoundStartTime.store(nowInMilli(), std::memory_order_relaxed);
    
        LOGI << "start runRoundOfflinePush gid size: " << newGroupMsgSeq.size()
             << ", redis db: " << toString(vecDbLists) ;
        
        // do check group list and push offline message
        for (const auto& itGroup : newGroupMsgSeq) {
            m_workThdPool.execInPool([this, itGroup, vecDbLists]() {
                handleOfflineGroupMessage(vecDbLists, itGroup.first, itGroup.second);
                m_runRoundTaskCount.fetch_sub(1, std::memory_order_seq_cst);
            });
        }
    }

    bool isLastRoundFinished()
    {
        return (0 == m_runRoundTaskCount.load());
    }
    
    uint64_t getStartTime()
    {
        return m_runRoundStartTime.load(std::memory_order_relaxed);
    }
    
private:
    void eventLoop()
    {
        setCurrentThreadName("sub.GroupEvent");
        int retVal = event_base_dispatch(m_eb);
        LOGI << "event_base_dispatch returned with value: " << retVal;
    }

    void handleMessage(const std::string& chan, const std::string& msg) override
    {
        LOGI << "subscription message received, channel: " << chan
             << ", message: " << msg;
        try {
            nlohmann::json msgObj = nlohmann::json::parse(msg);
            GroupEvent evt;
            msgObj.get_to(evt);

            if (m_groupMemberMgr.isGroupExist(evt.gid)) {
                handleEvent(evt.type, evt.uid, evt.gid);
            }

        } catch (std::exception& e) {
            LOGE << "json format error, channel: " << chan
                 << ", when handle message: " << msg
                 << ", exception caught: " << e.what();
        }
    }
    
    void handleEvent(int type, const std::string& uid, uint64_t gid)
    {
        switch (type) {
            case INTERNAL_USER_ENTER_GROUP:
                m_groupMemberMgr.handleUserEnterGroup(uid, gid);
                break;
            case INTERNAL_USER_QUIT_GROUP:
                m_groupMemberMgr.handleUserLeaveGroup(uid, gid);
                break;
            case INTERNAL_USER_MUTE_GROUP:
                m_groupMemberMgr.handleUserMuteGroup(uid, gid);
                break;
            case INTERNAL_USER_UNMUTE_GROUP:
                m_groupMemberMgr.handleUserUmuteGroup(uid, gid);
                break;
            default:
                LOGI << "group event type not process, gid: " << gid
                     << ", type: " << type
                     << ", uid: " << uid;
                break;
        }
    }
};

OfflineService::OfflineService(OfflineConfig& config,
                               std::shared_ptr<AccountsManager> accountMgr,
                               std::shared_ptr<OfflineServiceRegister> offlineReg,
                               std::map<int32_t, RedisConfig>& redisDbHosts,
                               std::shared_ptr<push::Service> pushService)
    : m_config(config)
      , m_execTime(0)
      , m_pImpl(new OfflineServiceImpl(config.redis[0], config, accountMgr, offlineReg, redisDbHosts, pushService))
      , m_impl(*m_pImpl)
      , m_masterLease("offline_redis_" + config.offlineSvr.redisPartition, 10000, OfflineService::lostLease)
{
    m_masterLease.start();
}

OfflineService::~OfflineService()
{
    m_masterLease.stop();
    if (nullptr != m_pImpl) {
        delete m_pImpl;
    }
}

void OfflineService::lostLease(void)
{
    LOGI << "OfflineService::lostLease: " << nowInSec();
}

// timer run
void OfflineService::run()
{
    int64_t start = nowInMilli();
    
    if (!m_masterLease.isMaster()) {
        LOGI << "OfflineService is not master, time: " << nowInSec();
        m_execTime = nowInMilli() - start;
        return;
    }
    
    if (m_impl.isLastRoundFinished()) {
        m_impl.runRoundOfflinePush();
    
        m_execTime = nowInMilli() - start;
    
        LOGI << "OfflineService::run start time: " << nowInSec()
             << ", run mill ms: " << m_execTime;
    } else {
        LOGE << "OfflineService::run is not finished. run time: "
                << (nowInMilli() - m_impl.getStartTime()) << "(ms)";
    }
}

void OfflineService::cancel()
{
}

int64_t OfflineService::lastExecTimeInMilli()
{
    return m_execTime;
}

} // namespace bcm