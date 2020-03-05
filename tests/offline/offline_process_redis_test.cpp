
#include "../test_common.h"
#include "fiber/fiber_timer.h"
#include "redis/redis_manager.h"
#include "config/redis_config.h"
#include "../../src/config/group_store_format.h"


#include "group/message_type.h"

#include "dao/client.h"

#include "../../src/utils/time.h"
#include "../../src/proto/dao/group_msg.pb.h"
#include "../../src/proto/dao/group_user.pb.h"
#include "../../src/proto/dao/sys_msg.pb.h"
#include "../../src/proto/dao/account.pb.h"


using namespace bcm;

void init(std::unordered_map<std::string, std::unordered_map<std::string, RedisConfig> > &redisDb) {
    RedisConfig c00 = {"127.0.0.1", 6376, "", ""}, c01 = {"127.0.0.1", 6377, "", ""},
            c10 = {"127.0.0.1", 6378, "", ""}, c11 = {"127.0.0.1", 6379, "", ""};
    std::unordered_map<std::string, RedisConfig> m0, m1;
    m0["0"] = c00;
    m0["1"] = c01;
    m1["0"] = c10;
    m1["1"] = c11;
    redisDb["0"] = m0;
    redisDb["1"] = m1;
}

void initializeDao()
{
    bcm::DaoConfig config;
    config.remote.hosts = "localhost:34567";
    config.remote.proto = "baidu_std";
    config.remote.connType = "";
    config.timeout = 60000;
    config.remote.retries = 3;
    config.remote.balancer = "";
    config.remote.keyPath = "./key.pem";
    config.remote.certPath = "./cert.pem";
    config.clientImpl = bcm::dao::REMOTE;
    
    REQUIRE(bcm::dao::initialize(config) == true);
}

static void __packAccountDevice(bcm::Device& newDev, bcm::GroupUserMessageIdInfo& gumi)
{
    newDev.set_id(1);
    newDev.set_name("111");
    newDev.set_authtoken("23t5365y5");
    newDev.set_salt("222");
    newDev.set_signalingkey("ddddddd");
    newDev.set_gcmid(gumi.gcmId);
    newDev.set_umengid(gumi.umengId);
    newDev.set_apnid(gumi.apnId);
    newDev.set_apntype(gumi.apnType);
    newDev.set_voipapnid(gumi.voipApnId);
    newDev.set_pushtimestamp(32432543265);
    newDev.set_fetchesmessages(true);
    newDev.set_registrationid(0);
    newDev.set_version(333);
    
    bcm::SignedPreKey  *tmS = newDev.mutable_signedprekey();
    tmS->set_keyid(22);
    tmS->set_publickey("3333");
    tmS->set_signature("233");
    
    newDev.set_lastseentime(3333333333);
    newDev.set_createtime(376547865);
    newDev.set_supportvideo(false);
    newDev.set_supportvoice(true);
    newDev.set_useragent("33435432");
    
    bcm::ClientVersion *cv = newDev.mutable_clientversion();
    cv->set_ostype(static_cast< ::bcm::ClientVersion_OSType >(gumi.osType));
    cv->set_osversion("vos");
    cv->set_phonemodel("ios");
    cv->set_bcmversion("bcmv23");
    cv->set_bcmbuildcode(gumi.bcmBuildCode);
}

static void __createAccount(std::shared_ptr<bcm::dao::Accounts> account, uint64_t uid, std::string sType)
{
    // init  account user
    bcm::Account acc;
    acc.set_uid(std::to_string(uid));
    acc.set_openid(std::to_string(uid));
    acc.set_publickey(std::to_string(uid));
    acc.set_identitykey(std::to_string(uid));
    acc.set_phonenumber(std::to_string(uid));
    acc.set_name(std::to_string(uid));
    acc.set_avater(std::to_string(uid));
    bcm::Device* tmpDev = acc.add_devices();
    
    bcm::GroupUserMessageIdInfo gumi;
    gumi.bcmBuildCode = 1200;
    gumi.osVersion  = "13.2";
    if (sType == "gcm") {
        gumi.gcmId = std::to_string(uid);
    } else if (sType == "umeng") {
        gumi.umengId = std::to_string(uid);
        gumi.osType = bcm::ClientVersion_OSType::ClientVersion_OSType_OSTYPE_ANDROID;
    } else if (sType == "apn") {
        gumi.osType = bcm::ClientVersion_OSType::ClientVersion_OSType_OSTYPE_ANDROID;
        gumi.apnId = std::to_string(uid);
        gumi.apnType = std::to_string(uid);
        gumi.voipApnId = std::to_string(uid);
    } else {
        return;
    }
    
    __packAccountDevice( *tmpDev, gumi);
    
    bcm::dao::ErrorCode  res;
    bcm::Account accOld;
    res = account->get(std::to_string(uid), accOld);
    if (bcm::dao::ERRORCODE_NO_SUCH_DATA == res) {
        res= account->create(acc);
        REQUIRE(bcm::dao::ERRORCODE_SUCCESS == res);
    }
}

static void __create_group(std::shared_ptr<bcm::dao::Groups> pGroups, uint64_t& gid)
{
    bcm::Group newGroup, newGroup1;
    newGroup.set_gid(444444);
    newGroup.set_lastmid(1);
    newGroup.set_name("groupPush");
    newGroup.set_icon("icon_path");
    newGroup.set_permission(1111);
    newGroup.set_updatetime(333333333);
    newGroup.set_broadcast(static_cast<bcm::Group_BroadcastStatus>(1));
    newGroup.set_status(1);
    newGroup.set_channel("channel");
    newGroup.set_intro("eeeeeeee");
    newGroup.set_createtime(2432413254);
    newGroup.set_key("ddfdsafdsafa");
    bcm::Group_EncryptStatus gEncryptStatus = static_cast<bcm::Group_EncryptStatus>(1);
    newGroup.set_encryptstatus(gEncryptStatus);
    newGroup.set_plainchannelkey("ddddd");
    newGroup.set_notice("{}");
    
    REQUIRE(pGroups->create(newGroup, gid) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
}

void onlineUserRedisCache(uint64_t gid, uint64_t mid, std::vector<std::string>& onlineUids){
    bcm::GroupUserMessageIdInfo redisDbUserMsgInfo;
    redisDbUserMsgInfo.last_mid = mid;
    std::string hkey = REDISDB_KEY_PREFIX_GROUP_USER_INFO + std::to_string(gid);
    std::vector<HField> hvalues;
    std::string val = redisDbUserMsgInfo.to_string();
    for (auto& u : onlineUids) {
        hvalues.push_back(HField(u, val));
    }
    if (!RedisDbManager::Instance()->hmset(gid, hkey, hvalues)) {
        std::cout << "failed to hmset users' mid to redis db, message: gid " << gid << ", mid " << mid << std::endl;
    }
}

void updateRedisdbOfflineInfo(uint64_t gid, uint64_t mid, GroupMultibroadMessageInfo& groupMultibroadInfo)
{
    bcm::PushPeopleType pushType = bcm::PushPeopleType::PUSHPEOPLETYPE_TO_ALL;
    if (!groupMultibroadInfo.members.empty()) {
        pushType = bcm::PushPeopleType::PUSHPEOPLETYPE_TO_DESIGNATED_PERSON;
        char field[50];
        snprintf(field, sizeof(field), "%020lu_%020lu_%02d", gid, mid, pushType);
        std::stringstream ss;
        ss << field;
        if (!RedisDbManager::Instance()->hset(gid, REDISDB_KEY_GROUP_MULTI_LIST_INFO, ss.str(), groupMultibroadInfo.to_string())) {
            std::cout  << "failed to hset group info to redis 'group_multi_msg_list', gid: " << gid
                 << ", mid: " << mid << ", broadInfo: " << groupMultibroadInfo.to_string()<< std::endl;
            return;
        }
    }
    
    char field[50];
    snprintf(field, sizeof(field), "%020lu_%020lu_%02d", gid, mid, pushType);
    std::stringstream ss;
    ss << field;
    if (!RedisDbManager::Instance()->zadd(gid, REDISDB_KEY_GROUP_MSG_INFO, ss.str(), nowInSec())) {
        std::cout  << "failed to zadd group info to redis 'group_msg_list', gid: " << gid
             << ", mid: " << mid << ", from_uid: " << groupMultibroadInfo.from_uid << std::endl;
        return;
    }
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
                                                                 cursor, "", 100,
                                                                 new_cursor, resultGroupUsers);
            if (!res) {
                TLOG << "redis group_user list get error, redisId: " << itRedis
                     << ", gid: " << gid
                     << ", key: " << REDISDB_KEY_PREFIX_GROUP_USER_INFO << std::to_string(gid)
                     << ", cursor: " << cursor << ", count: " << 100
                     << ", new_cursor: " << new_cursor << ", result: " << res
                     << ", return size: " << resultGroupUsers.size();
                break;
            }
            
            for (const auto& itUid : resultGroupUsers) {
                GroupUserMessageIdInfo guDb;
                if (!guDb.from_string(itUid.second)) {
                    TLOG << "redis group_user list format error, redisId: " << itRedis
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
    
            TLOG << "redis group_user list, redisId: " << itRedis
                 << ", gid: " << gid
                 << ", key: " << REDISDB_KEY_PREFIX_GROUP_USER_INFO + std::to_string(gid)
                 << ", cursor: " << cursor << ", count: " << 100
                 << ", new_cursor: " << new_cursor << ", result: " << res
                 << ", return size: " << resultGroupUsers.size();
            
            // update redis cursor
            cursor = new_cursor;
            
        } while ("0" != new_cursor);
    
        TLOG << "redis group_user one redis list, redisId: " << itRedis
             << ", gid: " << gid
             << ", key: " << REDISDB_KEY_PREFIX_GROUP_USER_INFO + std::to_string(gid)
             << ", group user size: " << redisGroupUserIndex;
        
    } // end for (const auto& itRedis : vecDbList)
    
    return true;
}

TEST_CASE("offline_redis_test") {
    std::unordered_map<std::string, std::unordered_map<std::string, RedisConfig> > redisDb;
    init(redisDb);
    
    std::cout << "www test ..." << std::endl;
    
    REQUIRE(6376 == redisDb["0"]["0"].port);
    REQUIRE(6377 == redisDb["0"]["1"].port);
    REQUIRE(6378 == redisDb["1"]["0"].port);
    REQUIRE(6379 == redisDb["1"]["1"].port);
    
    RedisDbManager::Instance()->setRedisDbConfig(redisDb);

    std::map<int32_t, RedisConfig> redisDbHosts;
    for (const auto& itDb : redisDb["0"]) {
        int32_t redisId = std::stoi(itDb.first);
        redisDbHosts[redisId] = itDb.second;
    }

    REQUIRE(RedisClientSync::OfflineInstance()->setRedisConfig(redisDbHosts) == true);
    
    //
    initializeDao();

    std::shared_ptr<bcm::dao::Accounts> m_accounts = bcm::dao::ClientFactory::accounts();
    REQUIRE(m_accounts != nullptr);
    
    int32_t  i;

    for (i = 10000; i < 10100; i++) {
        __createAccount(m_accounts, i, "gcm");
    }
    for (i = 10100; i < 10200; i++) {
        __createAccount(m_accounts, i, "umeng");
    }
    for (i = 10200; i < 10300; i++) {
        __createAccount(m_accounts, i, "apn");
    }

    std::shared_ptr<bcm::dao::GroupUsers> pgu = bcm::dao::ClientFactory::groupUsers();
    REQUIRE(pgu != nullptr);
    std::shared_ptr<bcm::dao::Groups> pGroups = bcm::dao::ClientFactory::groups();
    REQUIRE(pGroups != nullptr);

    uint64_t   gid = 1944924469105524833;
    
    bool isInsert = false;
    bcm::Group gidGroup;
    if (pGroups->get(gid, gidGroup) == bcm::dao::ErrorCode::ERRORCODE_NO_SUCH_DATA) {
        TLOG << "group id: " << gid << " is not exist!";
        __create_group(pGroups, gid);
        isInsert = true;
    }

    ::bcm::GroupUser gu;
    gu.set_gid(gid);
    gu.set_uid(std::to_string(10000));
    gu.set_nick(std::to_string(10000));
    gu.set_updatetime(123456789);
    gu.set_lastackmid(0);
    gu.set_role(bcm::GroupUser::Role::GroupUser_Role_ROLE_OWNER);
    gu.set_status(0);
    gu.set_encryptedkey("encryptedkey");
    gu.set_createtime(123456789);
    gu.set_nickname(std::to_string(10000));
    gu.set_profilekeys(std::to_string(10000));
    gu.set_groupnickname(std::to_string(10000));
    
    if (isInsert) {
        REQUIRE(pgu->insert(gu) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    }

    std::vector<std::string> onlineGroupUids;
    onlineGroupUids.emplace_back(std::to_string(10000));

    {
        std::vector<bcm::GroupUser> tmps;
        for (i = 10001; i < 10100; i++) {
            gu.set_uid(std::to_string(i));
            gu.set_nick(std::to_string(i));
            gu.set_role(bcm::GroupUser::Role::GroupUser_Role_ROLE_MEMBER);
            gu.set_groupnickname(std::to_string(i));
            tmps.push_back(gu);
            if (i < 10050) {
                onlineGroupUids.emplace_back(std::to_string(i));
            }
        }
        if (isInsert)
            REQUIRE(pgu->insertBatch(tmps) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    }

    {
        std::vector<bcm::GroupUser> tmps;
        for (i = 10100; i < 10200; i++) {
            gu.set_uid(std::to_string(i));
            gu.set_nick(std::to_string(i));
            gu.set_role(bcm::GroupUser::Role::GroupUser_Role_ROLE_MEMBER);
            gu.set_groupnickname(std::to_string(i));
            tmps.push_back(gu);
            if (i < 10150) {
                onlineGroupUids.emplace_back(std::to_string(i));
            }
        }
        if (isInsert)
            REQUIRE(pgu->insertBatch(tmps) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    }

    {
        std::vector<bcm::GroupUser> tmps;
        for (i = 10200; i < 10300; i++) {
            gu.set_uid(std::to_string(i));
            gu.set_nick(std::to_string(i));
            gu.set_role(bcm::GroupUser::Role::GroupUser_Role_ROLE_MEMBER);
            gu.set_groupnickname(std::to_string(i));
            tmps.push_back(gu);
            if (i < 10250) {
                onlineGroupUids.emplace_back(std::to_string(i));
            }
        }
        if (isInsert)
            REQUIRE(pgu->insertBatch(tmps) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    }

    int32_t  curMid = nowInSec();
    GroupMultibroadMessageInfo groupMultibroadInfo;

    updateRedisdbOfflineInfo(gid, curMid, groupMultibroadInfo);

    onlineUserRedisCache(gid, curMid, onlineGroupUids);

    
    groupMultibroadInfo.members.insert(std::to_string(10000));
    groupMultibroadInfo.from_uid = std::to_string(10001);
    updateRedisdbOfflineInfo(gid, curMid + 1, groupMultibroadInfo);
    
    sleep(10);
    
    //
    std::map<std::string /* uid */, GroupUserMessageIdInfo> groupUserMsgId;
    std::vector<uint32_t> vecDbList;
    vecDbList.push_back(0);

    dbScanGroupUserMsgList(vecDbList, gid, groupUserMsgId);
    REQUIRE(groupUserMsgId.size() == 300);
    REQUIRE(groupUserMsgId[std::to_string(10000)].last_mid == curMid+1);
    REQUIRE(groupUserMsgId[std::to_string(10000)].gcmId == std::to_string(10000));

    for (i = 10001; i < 10100; i++) {
        auto it = groupUserMsgId.find(std::to_string(i));
        if (it != groupUserMsgId.end()) {
            REQUIRE(it->second.last_mid == curMid);
            REQUIRE(it->second.umengId == "");
            REQUIRE(it->second.apnId == "");
            if (i < 10050) {
                REQUIRE(it->second.gcmId == "");
            } else {
                REQUIRE(it->second.gcmId == it->first);
            }
        } else {
            REQUIRE(i == curMid);
        }
    }

    for (i = 10100; i < 10200; i++) {
        auto it = groupUserMsgId.find(std::to_string(i));
        if (it != groupUserMsgId.end()) {
            REQUIRE(it->second.last_mid == curMid);
            REQUIRE(it->second.apnId == "");
            REQUIRE(it->second.gcmId == "");
            if (i < 10150) {
                REQUIRE(it->second.umengId == "");
            } else {
                REQUIRE(it->second.umengId == it->first);
            }
        } else {
            REQUIRE(i == curMid);
        }
    }

    for (i = 10200; i < 10300; i++) {
        auto it = groupUserMsgId.find(std::to_string(i));
        if (it != groupUserMsgId.end()) {
            REQUIRE(it->second.last_mid == curMid);
            REQUIRE(it->second.gcmId == "");
            REQUIRE(it->second.umengId == "");
            if (i < 10250) {
                REQUIRE(it->second.apnId == "");
            } else {
                REQUIRE(it->second.apnId == it->first);
            }
        } else {
            REQUIRE(i == curMid);
        }
    }
    
    //
    
    
}

