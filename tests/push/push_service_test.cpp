#include "../test_common.h"

#include "dao/client.h"
#include "store/accounts_manager.h"
#include "push/push_service.h"

#include "group/message_type.h"
#include "config/bcm_options.h"
#include <utils/jsonable.h>
#include "proto/dao/device.pb.h"
#include "../../src/utils/time.h"
#include "../../src/utils/sender_utils.h"
#include "../../src/crypto/base64.h"
#include "../../src/redis/redis_manager.h"

using namespace bcm;

struct PushTestConfig {
    LogConfig log;
    DaoConfig dao;
    std::vector<RedisConfig> redis;     // redis pub/sub
    std::unordered_map<std::string, std::unordered_map<std::string, RedisConfig>> groupRedis; // redis for store
    ApnsConfig apns;
    FcmConfig fcm;
    UmengConfig umeng;
    uint32_t    testPushNumb;
    std::vector<std::string>  testUids;
};

inline void to_json(nlohmann::json& j, const PushTestConfig& config)
{
    j = nlohmann::json{{"log", config.log},
                       {"dao", config.dao},
                       {"redis", config.redis},
                       {"groupRedis", config.groupRedis},
                       {"apns", config.apns},
                       {"fcm", config.fcm},
                       {"umeng", config.umeng},
                       {"testPushNumb", config.testPushNumb},
                       {"testUids", config.testUids}
    };
}

inline void from_json(const nlohmann::json& j, PushTestConfig& config)
{
    jsonable::toGeneric(j, "log", config.log);
    jsonable::toGeneric(j, "dao", config.dao);
    jsonable::toGeneric(j, "redis", config.redis);
    jsonable::toGeneric(j, "groupRedis", config.groupRedis);
    jsonable::toGeneric(j, "apns", config.apns);
    jsonable::toGeneric(j, "fcm", config.fcm);
    jsonable::toGeneric(j, "umeng", config.umeng);
    jsonable::toNumber(j, "testPushNumb", config.testPushNumb);
    jsonable::toGeneric(j, "testUids", config.testUids);
}



void readConfig(PushTestConfig& config, std::string configFile){
    std::cout << "read config file: " << configFile << std::endl;

    std::ifstream fin(configFile, std::ios::in);
    std::string content;
    char buf[4096] = {0};
    while (fin.good()) {
        fin.read(buf, 4096);
        content.append(buf, static_cast<unsigned long>(fin.gcount()));
    }

    nlohmann::json j = nlohmann::json::parse(content);
    config = j.get<PushTestConfig>();
    
    std::cout << "read config: " << j.dump() << std::endl;
}

struct SimpleGroupMemberInfo {
    std::string uid;
    std::string nick;
    GroupUser::Role role;
    
    SimpleGroupMemberInfo(const std::string& id, const std::string& nickName, GroupUser::Role userRole)
            :uid(id), nick(nickName), role(userRole){}
};

inline void to_json(nlohmann::json& j, const SimpleGroupMemberInfo& info)
{
    j = nlohmann::json{{"uid", info.uid},
                       {"nick", info.nick},
                       {"role", static_cast<int32_t>(info.role)}};
}

struct GroupSysMsgBody {
    GroupMemberUpdateAction action;
    std::vector<SimpleGroupMemberInfo> members;
};

inline void to_json(nlohmann::json& j, const GroupSysMsgBody& body)
{
    j = nlohmann::json{{"action", static_cast<int32_t>(body.action)},
                       {"members", body.members}};
}

int getSourceExtra(const std::string& uid, const std::string& publicKey, std::string& sSourceExtra)
{
    std::string encrypted;
    uint32_t version;
    std::string iv;
    std::string ephemeralPubkey;
    int ret = -1;
    if ((ret = SenderUtils::encryptSender(uid, publicKey, version, iv, ephemeralPubkey, encrypted)) != 0) {
        return ret;
    }
    nlohmann::json sourceExtra = nlohmann::json::object(
            {
                    {"version",         version},
                    {"groupMsgPubkey",  publicKey},
                    {"ephemeralPubkey", Base64::encode(ephemeralPubkey)},
                    {"iv",              Base64::encode(iv)},
                    {"source",          Base64::encode(encrypted)}
            });
    sSourceExtra = Base64::encode(sourceExtra.dump());
    return ret;
}

bool insertGroupMsg(std::shared_ptr<bcm::dao::GroupMsgs> pGroupMsg, uint64_t gid, uint64_t& newOutMid)
{
    // begin group message
    bcm::GroupMsg  newMsg;
    newMsg.set_gid(gid);
    newMsg.set_fromuid("16R6bk2VsqdWnJhs3qSgEhinMKr4UBUeEU");

    newMsg.set_updatetime(nowInSec());
    newMsg.set_createtime(nowInSec());
    newMsg.set_type(GroupMsg::TYPE_CHANNEL);
    newMsg.set_status(GroupMsg::STATUS_NORMAL);
    newMsg.set_atlist("[]");
    newMsg.set_verifysig("5+E7VP07brr9kuCYt+IqFvcsQKXhw05yJmq5t\\/Ri69oVZH+WFW+wXwAJKLEEKKCNIOWwz3OZvNRa9dM8XHJBgQ==");
    
    newMsg.set_text("{\"body\":\"NlcehMD5HNKRciY9PfHpvL6WKtJyMmB+SFBZGyAlhJ+cCtZgfabbmdJu61CYYR/YEGCa5DGipM05cc0uTv0Ic/y6UplRxZZfgSQ1OcSSpDAnIJU1NF9nKehP9RdBLEfPKuKnC+HxFguJ2oP8qDmyfP8prFtN/6aE5JJT+tyIk/eFNcVBmjlEH3maSnk2YcztW35EbcgNknYiWN+17oWhdZc3u+u67K6kwzhxjLvQUoM\\u003d\",\"header\":{\"encryption_level\":0,\"hash_data\":\"Sl80aInnEGway4ZqgrCn12DGk66PUGbuelkecbsagIQjgrWa+AdCNZGDixMOFyxuIlLsEsbLvo1DrLkUU8lFGw\\u003d\\u003d\"},\"version\":1}");
    
    std::string sSourceExtra = "";
    std::string sPubkey = "BRbIPtiIoas60hb86oRJsXCMqGa29sy+1bfm2dUsjZUO";
    if (0 !=getSourceExtra(newMsg.fromuid(), sPubkey, sSourceExtra)) {
        
        return false;
    }
    
    newMsg.set_sourceextra(sSourceExtra);
    
    REQUIRE(pGroupMsg->insert(newMsg, newOutMid) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    return true;
}

TEST_CASE("push_service") {
    PushTestConfig config;
    readConfig(config, "./config-offline-test.json");

    Log::init(config.log, "test-push");
    
    bool storeOk = dao::initialize(config.dao);
    REQUIRE(storeOk == true);

    // redis for group/offline
    REQUIRE(bcm::RedisDbManager::Instance()->setRedisDbConfig(config.groupRedis));

    std::shared_ptr<bcm::dao::Groups> pGroups = bcm::dao::ClientFactory::groups();
    REQUIRE(pGroups != nullptr);

    std::shared_ptr<bcm::dao::GroupMsgs> pGroupMsg = bcm::dao::ClientFactory::groupMsgs();
    REQUIRE(pGroupMsg != nullptr);
    
    std::shared_ptr<bcm::AccountsManager> accountMgr = std::make_shared<bcm::AccountsManager>();

    auto pushService = std::make_shared<PushService>(accountMgr, 
                        config.redis[0], config.apns, config.fcm, config.umeng);

    sleep(5);
    
    std::string gid = "8804760738657730713";
    /*
    std::vector<std::string> uids{
        "16R6bk2VsqdWnJhs3qSgEhinMKr4UBUeEU", // "huawei"
        "1MzeVPKBRwBoCBgWpQLgrQGmWTEJW7fzeW"  // "ios"
    }; */
    
    std::vector<Account> accountList;
    std::vector<std::string> missedUids;
    if (!accountMgr->get(config.testUids, accountList, missedUids)) {
        std::cerr << "accountMgr get failed !" << std::endl;
        return;
    }

    std::cout << "accountList size: " << accountList.size() << ", uids: " << toString(config.testUids) << std::endl;
    
    for (uint32_t i = 0; i < config.testPushNumb; i++) {
        uint64_t newOutMid = 0;
        if (!insertGroupMsg(pGroupMsg, std::stoull(gid), newOutMid)) {
            std::cerr << "insertGroupMsg get failed !" << std::endl;
            return;
        }
    
        std::cout << "accountList size: " << accountList.size() << ", mid: " << newOutMid << std::endl;
    
        for (auto& acc : accountList) {
            const auto& dev = AccountsManager::getDevice(acc, Device::MASTER_ID);
            if (dev == boost::none) {
                std::cout << "account, uid: " << acc.uid() << " does not have a master device" << std::endl;
                continue;
            }

            push::Notification notification;
            notification.group(gid, std::to_string(newOutMid));
            notification.setDeviceInfo(*dev);

            std::cout << "account, uid: " << acc.uid() << ", isSupportApnsPush: "
                      << notification.isSupportApnsPush() << std::endl;
            
            if (acc.uid() == "1MzeVPKBRwBoCBgWpQLgrQGmWTEJW7fzeW") {
                ClientVersion version = dev->clientversion();
                version.set_osversion("13.1");
                notification.setClientVersion(version);
                
                std::cout << "account 13.1 uid: " << acc.uid() << ", isSupportApnsPush: "
                            << notification.isSupportApnsPush() << std::endl;
            }
            
            pushService->sendNotification(notification.getPushType(), notification);
            
            std::cout << "account, uid: " << acc.uid() << " finished" << std::endl;
        }
    }
    
    std::cout << "push message finished" << std::endl;

    Log::flush();

    sleep(4);
}