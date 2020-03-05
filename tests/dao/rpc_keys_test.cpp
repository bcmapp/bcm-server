
#include <cinttypes>
#include "../test_common.h"
#include "dao/client.h"
#include "../../src/proto/dao/stored_message.pb.h"
#include "../../src/proto/dao/group_msg.pb.h"
#include "../../src/proto/dao/group_user.pb.h"
#include "../../src/proto/dao/sys_msg.pb.h"
#include "../../src/proto/dao/account.pb.h"
#include "../../src/proto/brpc/rpc_utilities.pb.h"
#include "dao/rpc_impl/group_keys_rpc_impl.h"
#include <thread>

void initialize()
{
    static bool initialized = false;
    if (initialized) {
        return;
    }
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
    
    initialized = true;
}

static inline int64_t nowInMillis()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();
}

TEST_CASE("onetime_key")
{
    initialize();

    std::shared_ptr<bcm::dao::OnetimeKeys> pOK = bcm::dao::ClientFactory::onetimeKeys();
    REQUIRE(pOK != nullptr);

    std::vector<bcm::OnetimeKey> keys1;
    for (int i = 1; i <= 10; i++) {
        bcm::OnetimeKey k;
        k.set_uid("uid1");
        k.set_deviceid(1);
        k.set_keyid(i);
        k.set_publickey("pk" + std::to_string(i));
        keys1.push_back(k);
    }

    std::vector<bcm::OnetimeKey> keys2;
    for (int i = 1; i <= 10; i++) {
        bcm::OnetimeKey k;
        k.set_uid("uid1");
        k.set_deviceid(2);
        k.set_keyid(i);
        k.set_publickey("pk" + std::to_string(i));
        keys2.push_back(k);
    }

    std::string sIdentityKey = "eeeeeeeeeeeeeeeee";
    bcm::SignedPreKey  tmSigned;
    tmSigned.set_keyid(22);
    tmSigned.set_publickey("3333");
    tmSigned.set_signature("233");
    
    REQUIRE(pOK->set("uid1", 1, keys1, sIdentityKey, tmSigned) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(pOK->set("uid1", 2, keys2, sIdentityKey, tmSigned) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    
    uint32_t count;
    REQUIRE(pOK->getCount("uid1", 1, count) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(count == 10);
    count = 0;
    REQUIRE(pOK->getCount("uid1", 2, count) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(count == 10);
    
    std::vector<bcm::OnetimeKey> keys;
    REQUIRE(pOK->get("uid1", keys) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    for (const auto& k : keys) {
        REQUIRE(k.uid() == "uid1");
        REQUIRE(k.keyid() == 1);
        REQUIRE(k.publickey() == "pk1");
        if (k.deviceid() != 1) {
            REQUIRE(k.deviceid() == 2);
        }
    }

    count = 0;
    REQUIRE(pOK->getCount("uid1", 1, count) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(count == 9);
    count = 0;
    REQUIRE(pOK->getCount("uid1", 2, count) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(count == 9);
    
    bcm::OnetimeKey key;
    REQUIRE(pOK->get("uid1", 1, key) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(key.uid() == "uid1");
    REQUIRE(key.deviceid() == 1);
    REQUIRE(key.keyid() == 2);
    REQUIRE(key.publickey() == "pk2");
    REQUIRE(pOK->get("uid1", 2, key) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(key.uid() == "uid1");
    REQUIRE(key.deviceid() == 2);
    REQUIRE(key.keyid() == 2);
    REQUIRE(key.publickey() == "pk2");
    
    count = 0;
    REQUIRE(pOK->getCount("uid1", 1, count) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(count == 8);
    count = 0;
    REQUIRE(pOK->getCount("uid1", 2, count) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(count == 8);
    
    REQUIRE(pOK->clear("uid1", 1) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    count = 0;
    REQUIRE(pOK->getCount("uid1", 1, count) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(count == 0);
    
    count = 0;
    REQUIRE(pOK->getCount("uid1", 2, count) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(count == 8);
    
    REQUIRE(pOK->clear("uid1") == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    
    count = 0;
    REQUIRE(pOK->getCount("uid1", 2, count) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(count == 0);
}

TEST_CASE("getKeys")
{
    initialize();
    std::set<std::string> uids = {
        "uid1",
        "uid2",
        "uid3",
        "uid4",
        "uid5"
    };
    std::map<std::string, bcm::Account> accounts;
    std::map<std::string, std::map<uint32_t, std::vector<bcm::OnetimeKey>>> keys;

    std::shared_ptr<bcm::dao::Accounts> daoAccounts = bcm::dao::ClientFactory::accounts();
    REQUIRE(daoAccounts != nullptr);
    std::shared_ptr<bcm::dao::OnetimeKeys> daoOnetimeKeys = bcm::dao::ClientFactory::onetimeKeys();
    REQUIRE(daoOnetimeKeys != nullptr);

    size_t deviceCount = 0;
    for (const auto& uid : uids) {
        bcm::Account acc;
        acc.set_uid(uid);
        acc.set_openid(uid);
        acc.set_publickey("publickey" + uid);
        acc.set_identitykey("identitykey" + uid);
        acc.set_phonenumber("phonenumber" + uid);
        acc.set_name("name" + uid);
        acc.set_avater("avatar" + uid);
        for (uint32_t i = 1; i <= 2; i++) {
            std::string deviceId;
            bcm::Device* dev = acc.add_devices();
            dev->set_id(i);
            dev->set_name("name" + deviceId);
            dev->set_authtoken("authtoken" + deviceId);
            dev->set_salt("salt" + deviceId);
            dev->set_signalingkey("signalingkey" + deviceId);
            dev->set_gcmid("gcmid" + deviceId);
            dev->set_apnid("apnid" + deviceId);
            dev->set_apntype("apntype" + deviceId);
            dev->set_voipapnid("voipapnid" + deviceId);
            dev->set_pushtimestamp(i);
            dev->set_fetchesmessages(true);
            dev->set_registrationid(i);
            dev->set_version(i);

            bcm::SignedPreKey  *k = dev->mutable_signedprekey();
            k->set_keyid(i);
            k->set_publickey("signedprekey.publickey" + deviceId);
            k->set_signature("signedprekey.signature" + deviceId);

            int64_t now = nowInMillis();
            dev->set_lastseentime(now);
            dev->set_createtime(now);
            dev->set_supportvideo(false);
            dev->set_supportvoice(true);
            dev->set_useragent("useragent" + deviceId);

            bcm::ClientVersion *cv = dev->mutable_clientversion();
            cv->set_ostype(bcm::ClientVersion_OSType::ClientVersion_OSType_OSTYPE_IOS);
            cv->set_osversion("vos");
            cv->set_phonemodel("ios");
            cv->set_bcmversion("1.20.1");
            cv->set_bcmbuildcode(9527);

            for (int j = 0; j < 10; j++) {
                bcm::OnetimeKey k;
                k.set_uid(uid);
                k.set_deviceid(i);
                k.set_keyid(j);
                k.set_publickey("onetimekey.publickey" + std::to_string(j));
                keys[uid][i].emplace_back(std::move(k));
            }
            REQUIRE(bcm::dao::ErrorCode::ERRORCODE_SUCCESS == daoOnetimeKeys->set(uid, i, keys[uid][i], acc.identitykey(), dev->signedprekey()));
            deviceCount++;
        }

        REQUIRE(bcm::dao::ErrorCode::ERRORCODE_SUCCESS == daoAccounts->create(acc));
        accounts.emplace(uid, std::move(acc));
    }

    auto signedPrekeyComp = [](const bcm::SignedPreKey& l, const bcm::SignedPreKey& r) -> void {
        REQUIRE(l.keyid() == r.keyid());
        REQUIRE(l.publickey() == r.publickey());
        REQUIRE(l.signature() == r.signature());
    };

    auto onetimeKeyComp = [](const bcm::OnetimeKey& l, const bcm::OnetimeKey& r) -> void {
        REQUIRE(l.uid() == r.uid());
        REQUIRE(l.deviceid() == r.deviceid());
        REQUIRE(l.keyid() == r.keyid());
        REQUIRE(l.publickey() == r.publickey());
    };

    std::vector<bcm::Keys> keysResult;
    REQUIRE(bcm::dao::ErrorCode::ERRORCODE_SUCCESS == daoAccounts->getKeys(uids, keysResult));
    REQUIRE(deviceCount == keysResult.size());
    for (const auto& item : keysResult) {
        auto itUser = accounts.find(item.uid());
        REQUIRE(itUser != accounts.end());
        auto device = itUser->second.devices(item.deviceid() - 1);
        REQUIRE(device.id() == item.deviceid());
        //REQUIRE(device.registrationid() == item.registrationid());
        REQUIRE(itUser->second.identitykey() == item.identitykey());
        signedPrekeyComp(device.signedprekey(), item.signedprekey());
        onetimeKeyComp(keys[item.uid()][device.id()].front(), item.onetimekey());
    }
}

TEST_CASE("getKeysByGid")
{
    initialize();
    std::set<std::string> uids = {
        "uid11",
        "uid21",
        "uid31",
        "uid41",
        "uid51"
    };
    std::vector<bcm::Group> groups;
    std::map<std::string, bcm::Account> accounts;
    std::map<std::string, std::map<uint32_t, std::vector<bcm::OnetimeKey>>> keys;

    std::shared_ptr<bcm::dao::GroupUsers> daoGroupUsers = bcm::dao::ClientFactory::groupUsers();
    REQUIRE(daoGroupUsers != nullptr);
    std::shared_ptr<bcm::dao::Groups> daoGroups = bcm::dao::ClientFactory::groups();
    REQUIRE(daoGroups != nullptr);
    std::shared_ptr<bcm::dao::Accounts> daoAccounts = bcm::dao::ClientFactory::accounts();
    REQUIRE(daoAccounts != nullptr);
    std::shared_ptr<bcm::dao::OnetimeKeys> daoOnetimeKeys = bcm::dao::ClientFactory::onetimeKeys();
    REQUIRE(daoOnetimeKeys != nullptr);

    bcm::Group group;
    group.set_gid(1);
    group.set_lastmid(0);
    group.set_name("group1");
    group.set_icon("icon_path");
    group.set_permission(1);
    group.set_updatetime(nowInMillis());
    bcm::Group_BroadcastStatus  gBroadStatus = static_cast<bcm::Group_BroadcastStatus>(1);
    group.set_broadcast(gBroadStatus);
    group.set_status(1);
    group.set_channel("");
    group.set_intro("intro");
    group.set_createtime(group.updatetime());
    group.set_key("group.key");
    bcm::Group_EncryptStatus gEncryptStatus = static_cast<bcm::Group_EncryptStatus>(1);
    group.set_encryptstatus(gEncryptStatus);
    group.set_plainchannelkey("");
    group.set_notice("{}");
    group.set_shareqrcodesetting("shareqrcodesetting" + std::to_string(group.gid()));
    group.set_ownerconfirm(1);
    group.set_sharesignature("sharesignature" + std::to_string(group.gid()));
    group.set_shareandownerconfirmsignature("shareandownerconfirmsignature" + std::to_string(group.gid()));
    group.set_version(1);
    group.set_encryptedgroupinfosecret("encryptedgroupinfosecret" + std::to_string(group.gid()));
    group.set_encryptedephemeralkey("encryptedephemeralkey" + std::to_string(group.gid()));

    uint64_t gid;
    REQUIRE(bcm::dao::ErrorCode::ERRORCODE_SUCCESS == daoGroups->create(group, gid));
    group.set_gid(gid);

    size_t deviceCount = 0;
    for (const auto& uid : uids) {
        bcm::Account acc;
        acc.set_uid(uid);
        acc.set_openid(uid);
        acc.set_publickey("publickey" + uid);
        acc.set_identitykey("identitykey" + uid);
        acc.set_phonenumber("phonenumber" + uid);
        acc.set_name("name" + uid);
        acc.set_avater("avatar" + uid);
        for (uint32_t i = 1; i <= 2; i++) {
            std::string deviceId;
            bcm::Device* dev = acc.add_devices();
            dev->set_id(i);
            dev->set_name("name" + deviceId);
            dev->set_authtoken("authtoken" + deviceId);
            dev->set_salt("salt" + deviceId);
            dev->set_signalingkey("signalingkey" + deviceId);
            dev->set_gcmid("gcmid" + deviceId);
            dev->set_apnid("apnid" + deviceId);
            dev->set_apntype("apntype" + deviceId);
            dev->set_voipapnid("voipapnid" + deviceId);
            dev->set_pushtimestamp(i);
            dev->set_fetchesmessages(true);
            dev->set_registrationid(i);
            dev->set_version(i);

            bcm::SignedPreKey  *k = dev->mutable_signedprekey();
            k->set_keyid(i);
            k->set_publickey("signedprekey.publickey" + deviceId);
            k->set_signature("signedprekey.signature" + deviceId);

            int64_t now = nowInMillis();
            dev->set_lastseentime(now);
            dev->set_createtime(now);
            dev->set_supportvideo(false);
            dev->set_supportvoice(true);
            dev->set_useragent("useragent" + deviceId);

            bcm::ClientVersion *cv = dev->mutable_clientversion();
            cv->set_ostype(bcm::ClientVersion_OSType::ClientVersion_OSType_OSTYPE_IOS);
            cv->set_osversion("vos");
            cv->set_phonemodel("ios");
            cv->set_bcmversion("1.20.1");
            cv->set_bcmbuildcode(9527);

            for (int j = 0; j < 10; j++) {
                bcm::OnetimeKey k;
                k.set_uid(uid);
                k.set_deviceid(i);
                k.set_keyid(j);
                k.set_publickey("onetimekey.publickey" + std::to_string(j));
                keys[uid][i].emplace_back(std::move(k));
            }
            REQUIRE(bcm::dao::ErrorCode::ERRORCODE_SUCCESS == daoOnetimeKeys->set(uid, i, keys[uid][i], acc.identitykey(), dev->signedprekey()));
            deviceCount++;
        }

        REQUIRE(bcm::dao::ErrorCode::ERRORCODE_SUCCESS == daoAccounts->create(acc));
        accounts.emplace(uid, std::move(acc));
    }

    for (const auto& acc : accounts) {
        ::bcm::GroupUser gu;
        gu.set_gid(gid);
        gu.set_uid(acc.first);
        gu.set_nick(gu.uid());
        gu.set_updatetime(nowInMillis());
        gu.set_lastackmid(0);
        gu.set_role(bcm::GroupUser::Role::GroupUser_Role_ROLE_OWNER);
        gu.set_status(0);
        gu.set_encryptedkey("");
        gu.set_createtime(nowInMillis());
        gu.set_nickname("");
        gu.set_profilekeys("");
        gu.set_groupnickname(gu.uid());
        gu.set_groupinfosecret("groupinfosecret" + std::to_string(gu.gid()) + "_" + gu.uid());
        gu.set_proof("proof" + std::to_string(gu.gid()) + "_" + gu.uid());

        REQUIRE(daoGroupUsers->insert(gu) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    }

    auto signedPrekeyComp = [](const bcm::SignedPreKey& l, const bcm::SignedPreKey& r) -> void {
        REQUIRE(l.keyid() == r.keyid());
        REQUIRE(l.publickey() == r.publickey());
        REQUIRE(l.signature() == r.signature());
    };

    auto onetimeKeyComp = [](const bcm::OnetimeKey& l, const bcm::OnetimeKey& r) -> void {
        REQUIRE(l.uid() == r.uid());
        REQUIRE(l.deviceid() == r.deviceid());
        REQUIRE(l.keyid() == r.keyid());
        REQUIRE(l.publickey() == r.publickey());
    };

    std::vector<bcm::Keys> keysResult;
    REQUIRE(bcm::dao::ErrorCode::ERRORCODE_SUCCESS == daoAccounts->getKeys(uids, keysResult));
    REQUIRE(deviceCount == keysResult.size());
    for (const auto& item : keysResult) {
        auto itUser = accounts.find(item.uid());
        REQUIRE(itUser != accounts.end());
        auto device = itUser->second.devices(item.deviceid() - 1);
        REQUIRE(device.id() == item.deviceid());
        //REQUIRE(device.registrationid() == item.registrationid());
        REQUIRE(itUser->second.identitykey() == item.identitykey());
        signedPrekeyComp(device.signedprekey(), item.signedprekey());
        onetimeKeyComp(keys[item.uid()][device.id()].front(), item.onetimekey());
    }
}
