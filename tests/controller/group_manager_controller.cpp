#include "../test_common.h"
#include "../../src/controllers/group_manager_controller.h"
#include "mock.h"
#include "../../src/http/custom_http_status.h"
#include "../../src/config/group_config.h"
#include "../../src/config/multi_device_config.h"
#include "../../src/utils/json_serializer.h"
#include <string>
#include <iostream>
#include <metrics_client.h>
#include "../../src/proto/dao/group_keys.pb.h"
#include "../../src/limiters/distributed_limiter.h"

using namespace bcm;
using namespace bcm::metrics;

void initMetricsClientGroup() {

    std::string rmStr = "exec rm -rf /tmp/bcm_metrics_*";
    system(rmStr.c_str());

    MetricsConfig config;
    config.appVersion = "1.0";
    config.reportQueueSize = 5000;
    config.metricsDir = "/tmp";
    config.metricsFileSizeInBytes = 1024*10;
    config.metricsFileCount = 5;
    config.reportIntervalInMs = 3000;
    config.clientId = "00001";
    config.writeThresholdInBytes = 1024 * 1024;
    MetricsClient::Init(config);
}

void calculateSignature(const std::string& key, const std::string& text, std::string& encodedSignature)
{
    signal_context* context = nullptr;
    signal_context_create(&context, nullptr);
    signal_context_set_crypto_provider(context, &openssl_provider);

    ec_private_key* privateKey = nullptr;
    std::string decKey = bcm::Base64::decode(key);
    curve_decode_private_point(&privateKey, reinterpret_cast<const uint8_t*>(decKey.data()), decKey.size(), context);

    signal_buffer* signature = nullptr;
    curve_calculate_signature(context, &signature, privateKey,
                              reinterpret_cast<const uint8_t*>(text.data()), text.size());
    const char* tmp = (const char*)signal_buffer_data(signature);
    encodedSignature = bcm::Base64::encode(std::string(tmp, tmp + signal_buffer_len(signature)));
}

void generateGroupSetting(uint64_t gid,
                          const std::string& privkey,
                          std::string& setting,
                          std::string& settingSig,
                          int& ownerConfirm,
                          std::string& settingConfirmSig)
{
    std::string plainSetting = "setting for " + std::to_string(gid);
    setting = bcm::Base64::encode(plainSetting);
    calculateSignature(privkey, plainSetting, settingSig);
    std::ostringstream oss;
    oss << plainSetting << ownerConfirm;
    calculateSignature(privkey, oss.str(), settingConfirmSig);
}

TEST_CASE("onGroupJoinRequst")
{
    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    GroupConfig groupConfig;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    AccountMock* accountMock = new AccountMock();
    GroupsMock* groupsMock = new GroupsMock();
    GroupUsersMock* groupUsersMock = new GroupUsersMock(*groupsMock, *accountMock);
    PendingGroupUsersMock* pendingGroupUsersMock = new PendingGroupUsersMock();
    GroupMsgMock* groupMsg = new GroupMsgMock();
    controller.m_accountsManager->m_accounts.reset(accountMock);
    controller.m_groups.reset(groupsMock);
    controller.m_groupUsers.reset(groupUsersMock);
    controller.m_pendingGroupUsers.reset(pendingGroupUsersMock);
    controller.m_groupMessage.reset(groupMsg);

    HttpContext context;
    bcm::GroupJoinRequest req;
    req.gid = groupsMock->m_groups.begin()->first;
    std::string ownerUid;
    REQUIRE(groupUsersMock->getGroupOwner(req.gid, ownerUid) == bcm::dao::ERRORCODE_SUCCESS);
    bcm::GroupUser owner;
    REQUIRE(groupUsersMock->getMember(req.gid, ownerUid, owner) == bcm::dao::ERRORCODE_SUCCESS);

    std::string setting;
    std::string settingSig;
    int ownerConfirm = 0;
    std::string settingConfirmSig;
    generateGroupSetting(req.gid,
                         PrivateKeyStore::getInstance().get(ownerUid),
                         setting,
                         settingSig,
                         ownerConfirm,
                         settingConfirmSig);
    groupsMock->m_groups[req.gid].set_shareqrcodesetting(setting);
    groupsMock->m_groups[req.gid].set_ownerconfirm(ownerConfirm);
    groupsMock->m_groups[req.gid].set_sharesignature(settingSig);
    groupsMock->m_groups[req.gid].set_shareandownerconfirmsignature(settingConfirmSig);

    std::ostringstream oss;
    oss << "code" << bcm::Base64::decode(settingSig);
    bcm::Account account;
    auto it = groupUsersMock->m_groupUsers.find(req.gid);
    REQUIRE(it != groupUsersMock->m_groupUsers.end());
    for (const auto& item : it->second) {
        if (item.second.uid() != ownerUid) {
            REQUIRE(accountMock->get(item.second.uid(), account) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
            break;
        }
    }
    req.qrCode = bcm::Base64::encode("code");
    req.qrCodeToken = settingSig;
    calculateSignature(PrivateKeyStore::getInstance().get(account.uid()), oss.str(), req.signature);
    req.comment = "";

    context.authResult = account;
    context.requestEntity = req;
    bcm::GroupUser gu = groupUsersMock->m_groupUsers[req.gid].at(account.uid());
    groupUsersMock->m_groupUsers[req.gid].erase(account.uid());

    // on db error
    groupsMock->setErrorCode(bcm::dao::ErrorCode::ERRORCODE_NO_SUCH_DATA);
    controller.onGroupJoinRequst(context);
    REQUIRE(context.response.result() == http::status::bad_request);

    groupsMock->setErrorCode(bcm::dao::ErrorCode::ERRORCODE_INTERNAL_ERROR);
    controller.onGroupJoinRequst(context);
    REQUIRE(context.response.result() == http::status::internal_server_error);

    groupsMock->setErrorCode(bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    accountMock->setErrorCode(bcm::dao::ErrorCode::ERRORCODE_NO_SUCH_DATA);
    controller.onGroupJoinRequst(context);
    REQUIRE(context.response.result() == http::status::internal_server_error);

    accountMock->setErrorCode(bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    groupUsersMock->setErrorCode(bcm::dao::ErrorCode::ERRORCODE_NO_SUCH_DATA);
    controller.onGroupJoinRequst(context);
    REQUIRE(context.response.result() == http::status::internal_server_error);

    groupUsersMock->setErrorCode(bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    pendingGroupUsersMock->setErrorCode(bcm::dao::ErrorCode::ERRORCODE_NO_SUCH_DATA);
    controller.onGroupJoinRequst(context);
    REQUIRE(context.response.result() == http::status::internal_server_error);

    pendingGroupUsersMock->setErrorCode(bcm::dao::ErrorCode::ERRORCODE_SUCCESS);

    // signature error
    req.qrCodeToken += "error";
    context.requestEntity = req;
    controller.onGroupJoinRequst(context);
    REQUIRE(context.response.result() == http::status::bad_request);

    req.qrCodeToken = settingSig;

    // db data error
    context.requestEntity = req;
    groupsMock->m_groups[req.gid].set_sharesignature("error");
    controller.onGroupJoinRequst(context);
    REQUIRE(context.response.result() == http::status::bad_request);

    groupsMock->m_groups[req.gid].set_sharesignature(settingSig);

    // happpy path when ownerConfirm=0
    groupUsersMock->m_groupUsers[req.gid].erase(account.uid());
    controller.onGroupJoinRequst(context);
    REQUIRE(context.response.result() == http::status::ok);

    // happy path when ownerConfirm = 1
    ownerConfirm = 1;
    generateGroupSetting(req.gid,
                         PrivateKeyStore::getInstance().get(ownerUid),
                         setting,
                         settingSig,
                         ownerConfirm,
                         settingConfirmSig);
    groupsMock->m_groups[req.gid].set_shareqrcodesetting(setting);
    groupsMock->m_groups[req.gid].set_ownerconfirm(ownerConfirm);
    groupsMock->m_groups[req.gid].set_sharesignature(settingSig);
    groupsMock->m_groups[req.gid].set_shareandownerconfirmsignature(settingConfirmSig);

    oss.str("");
    oss << "code" << bcm::Base64::decode(settingSig);
    req.qrCode = bcm::Base64::encode("code");
    req.qrCodeToken = settingSig;
    calculateSignature(PrivateKeyStore::getInstance().get(account.uid()), oss.str(), req.signature);
    req.comment = "";

    context.requestEntity = req;
    groupUsersMock->m_groupUsers[req.gid].erase(account.uid());
    controller.onGroupJoinRequst(context);
    REQUIRE(context.response.result() == http::status::ok);

    // the user is already in group
    groupUsersMock->m_groupUsers[req.gid][account.uid()] = gu;
    controller.onGroupJoinRequst(context);
    REQUIRE(context.response.result() == http::status::ok);
}

TEST_CASE("create_group")
{
    initMetricsClientGroup();
    bcm::RedisConfig redisCfg;
    redisCfg.ip = "127.0.0.1";
    redisCfg.port = 6379;    
    redisCfg.password = "";
    redisCfg.regkey = "";

    std::unordered_map<std::string, std::unordered_map<std::string, RedisConfig> > configs;
    configs["p0"]["0"] = redisCfg;
    configs["p0"]["1"] = redisCfg;

    RedisDbManager::Instance()->setRedisDbConfig(configs);
    size_t limit = 20;
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    GroupConfig groupConfig;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    AccountMock* accountMock = new AccountMock();
    GroupsMock* groupsMock = new GroupsMock();
    GroupUsersMock* groupUsersMock = new GroupUsersMock(*groupsMock, *accountMock);
    PendingGroupUsersMock* pendingGroupUsersMock = new PendingGroupUsersMock();
    GroupMsgMock* groupMsg = new GroupMsgMock();
    controller.m_accountsManager->m_accounts.reset(accountMock);
    controller.m_groups.reset(groupsMock);
    controller.m_groupUsers.reset(groupUsersMock);
    controller.m_pendingGroupUsers.reset(pendingGroupUsersMock);
    controller.m_groupMessage.reset(groupMsg);
    bcm::DistributedLimiter* limiter = (bcm::DistributedLimiter*)controller.m_groupCreationLimiter.get();
    limiter->m_rule.period = 10000;

    bcm::Group group;
    groupsMock->generateGroup(group);
    std::string setting;
    std::string settingSig;
    int ownerConfirm = 0;
    std::string settingConfirmSig;
    bcm::Account account = accountMock->m_accounts.begin()->second;
    generateGroupSetting(0,
                         PrivateKeyStore::getInstance().get(account.uid()),
                         setting,
                         settingSig,
                         ownerConfirm,
                         settingConfirmSig);

    CreateGroupBodyV2 req;
    req.name = "";
    req.icon = "";
    req.intro = "";
    req.ownerKey = "ownerKey";
    req.ownerSecretKey = "ownerSecretKey";
    int i = 0;
    for (const auto& item : accountMock->m_accounts) {
        if (item.first != account.uid()) {
            req.members.emplace_back(item.second.uid());
            req.membersGroupInfoSecrets.emplace_back(item.second.uid());
            req.memberKeys.emplace_back(item.second.uid());
            if (i == 3) {
                break;
            }
        }
    }
    req.qrCodeSetting = setting;
    req.ownerConfirm = ownerConfirm;
    req.shareSignature = settingSig;
    req.shareAndOwnerConfirmSignature = settingConfirmSig;

    HttpContext context;
    context.authResult = account;
    context.requestEntity = req;

    // success within limit
    for (uint64_t i = 0; i < limit; i++) {
        controller.onCreateGroupV2(context);
        REQUIRE(context.response.result() == http::status::ok);
    }

    // failed beyond limit
    controller.onCreateGroupV2(context);
    REQUIRE(context.response.result_int() == static_cast<int>(bcm::custom_http_status::limiter_rejected));

    sleep(10);

    // success after interval
    controller.onCreateGroupV2(context);
    REQUIRE(context.response.result() == http::status::ok);

}



TEST_CASE("onQueryMemberListOrdered")
{
    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    GroupConfig groupConfig;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    AccountMock* accountMock = new AccountMock();
    GroupsMock* groupsMock = new GroupsMock();
    GroupUsersMock* groupUsersMock = new GroupUsersMock(*groupsMock, *accountMock);
    PendingGroupUsersMock* pendingGroupUsersMock = new PendingGroupUsersMock();
    GroupMsgMock* groupMsg = new GroupMsgMock();
    controller.m_accountsManager->m_accounts.reset(accountMock);
    controller.m_groups.reset(groupsMock);
    controller.m_groupUsers.reset(groupUsersMock);
    controller.m_pendingGroupUsers.reset(pendingGroupUsersMock);
    controller.m_groupMessage.reset(groupMsg);

    QueryMemberListOrderedBody req;
    req.gid = groupUsersMock->m_groupUsers.begin()->first;
    req.startUid = "";
    req.createTime = 0;
    req.count = 2;    

    std::map<std::string, bcm::GroupUser> ordered;
    groupUsersMock->getOrderedUsers(req.gid, ordered);

    HttpContext context;
    bcm::Account account;
    accountMock->get(ordered.begin()->second.uid(), account);
    context.authResult = account;
    context.requestEntity = req;

    size_t pos = 0;
    // happy path    
    do {
        controller.onQueryMemberListOrdered(context);
        REQUIRE(context.response.result() == http::status::ok);
        GroupResponse* resp = boost::any_cast<GroupResponse>(&context.responseEntity);
        struct Item {
            std::string uid;
            int64_t createTime;
        };
        std::vector<Item> result;
        for (auto& x : resp->result.find("members")->items()) {
            Item item;
            item.uid = x.value().find("uid")->get<std::string>();
            item.createTime = x.value().find("createTime")->get<int64_t>();
            result.emplace_back(std::move(item));
        }
        if (result.empty()) {
            break;
        }
        req.startUid = result.back().uid;
        req.createTime = result.back().createTime;
        size_t i = 0;
        for (const auto& it : ordered) {
            if (i < pos) {
                i++;
                continue;
            }
            if (i == pos + result.size()) {
                break;
            }
            REQUIRE(result[i - pos].uid == it.second.uid());
            REQUIRE(result[i - pos].createTime == it.second.createtime());
            i++;
        }
        pos += result.size();
        context.requestEntity = req;
    } while (true);

    // invalid parameters
    req.roles.emplace_back(0);
    req.roles.emplace_back(1);
    req.roles.emplace_back(2);
    req.roles.emplace_back(3);
    req.roles.emplace_back(4);
    req.roles.emplace_back(5);
    context.requestEntity = req;
    controller.onQueryMemberListOrdered(context);   
    REQUIRE(context.response.result() == http::status::bad_request);

    req.roles.clear();
    req.roles.emplace_back(1);
    req.count = 0;
    context.requestEntity = req;
    controller.onQueryMemberListOrdered(context);
    REQUIRE(context.response.result() == http::status::bad_request);

    req.count = 501;
    context.requestEntity = req;
    controller.onQueryMemberListOrdered(context);
    REQUIRE(context.response.result() == http::status::bad_request);

    // db error
    req.count = 500;
    context.requestEntity = req;
    groupUsersMock->setErrorCode(bcm::dao::ERRORCODE_NO_SUCH_DATA);
    controller.onQueryMemberListOrdered(context);
    REQUIRE(context.response.result() == http::status::bad_request);

    groupUsersMock->setErrorCode(bcm::dao::ERRORCODE_INTERNAL_ERROR);
    controller.onQueryMemberListOrdered(context);
    REQUIRE(context.response.result() == http::status::internal_server_error);

}

TEST_CASE("testCreateGroupV3Deserialize")
//void testCreateGroupV3()
{
    std::string requestStr = "{\n"
                             "\t\"name\" : \"name1\",\n"
                             "\t\"icon\" : \"icon1\",\n"
                             "\t\"intro\" : \"intro1\",\n"
                             "\t\"broadcast\" : 1,\n"
                             "\t\"owner_group_info_secret\" : \"ownerSecretKey1\",\n"
                             "\t\"members\" : [ \"m1\", \"m2\", \"m3\" ],\n"
                             "\t\"member_group_info_secrets\" : [\"mgis1\", \"mgis2\", \"mgis3\"],\n"
                             "\t\"share_qr_code_setting\" : \"share_qr_code_setting1\",\n"
                             "\t\"owner_confirm\" : 1,\n"
                             "\t\"share_sig\" : \"share_sig1\",\n"
                             "\t\"share_and_owner_confirm_sig\" : \"share_and_owner_confirm_sig1\",\n"
                             "\t\"encrypted_group_info_secret\" : \"encrypted_group_info_secret1\",\n"
                             "\t\"group_keys\" : {\n"
                             "\t\t\"encrypt_version\" : 1,\n"
                             "\t\t\"keys_v0\" : [\n"
                             "\t\t\t{\n"
                             "\t\t\t\t\"uid\": \"uid1\",\n"
                             "\t\t\t\t\"device_id\": 1,\n"
                             "\t\t\t\t\"key\" : \"key1\"\n"
                             "\t\t\t},\n"
                             "\t\t\t{\n"
                             "\t\t\t\t\"uid\": \"uid1\",\n"
                             "\t\t\t\t\"device_id\": 2,\n"
                             "\t\t\t\t\"key\" : \"key11\"\n"
                             "\t\t\t},\n"
                             "\t\t\t{\n"
                             "\t\t\t\t\"uid\": \"uid2\",\n"
                             "\t\t\t\t\"device_id\": 1,\n"
                             "\t\t\t\t\"key\" : \"key2\"\n"
                             "\t\t\t},\n"
                             "\t\t\t{\n"
                             "\t\t\t\t\"uid\": \"uid3\",\n"
                             "\t\t\t\t\"device_id\": 1,\n"
                             "\t\t\t\t\"key\" : \"key3\"\n"
                             "\t\t\t},\n"
                             "\t\t\t{\n"
                             "\t\t\t\t\"uid\": \"uid4\",\n"
                             "\t\t\t\t\"device_id\": 1,\n"
                             "\t\t\t\t\"key\" : \"key4\"\n"
                             "\t\t\t}\n"
                             "\t\t],\n"
                             "\t\t\"keys_v1\" : {\n"
                             "\t\t\t\"key\" : \"keyv1\"\n"
                             "\t\t}\n"
                             "\t},\n"
                             "\t\"member_proofs\" : [\"mp1\", \"mp2\", \"mp3\"],\n"
                             "\t\"owner_proof\" : \"owner_proof1\",\n"
                             "\t\"group_keys_mode\" : 1,\n"
                             "\t\"encrypted_ephemeral_key\" : \"encrypted_ephemeral_key1\"\n"
                             "}";

    JsonSerializerImp<CreateGroupBodyV3> jsonSerializerImp;
    boost::any entity{};
    if (!jsonSerializerImp.deserialize(requestStr, entity)) {
        std::cout << "CreateGroupBodyV3 deserialize fail! " << std::endl;
        return;
    }
    auto* createGroupBodyV3 = boost::any_cast<CreateGroupBodyV3>(&entity);
    std::cout << "CreateGroupBodyV3 deserialize success! " << nlohmann::json(createGroupBodyV3->groupKeys).dump() << std::endl;

    REQUIRE(createGroupBodyV3->encryptedGroupInfoSecret == "encrypted_group_info_secret1");
    REQUIRE(createGroupBodyV3->ownerProof == "owner_proof1");
    REQUIRE(createGroupBodyV3->groupKeysMode == 1);
    REQUIRE(createGroupBodyV3->encryptedEphemeralKey == "encrypted_ephemeral_key1");
    REQUIRE(createGroupBodyV3->groupKeys.keysV0.size() == 5);
    REQUIRE(createGroupBodyV3->groupKeys.encryptVersion == 1);
    REQUIRE(createGroupBodyV3->groupKeys.keysV0[0].uid == "uid1");
    REQUIRE(createGroupBodyV3->groupKeys.keysV0[0].key == "key1");
    REQUIRE(createGroupBodyV3->groupKeys.keysV0[0].deviceId == 1);
    REQUIRE(createGroupBodyV3->memberProofs.size() == 3);
}


void testCreateGroupV3()
{

    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    GroupConfig groupConfig;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    AccountMock* accountMock = new AccountMock();
    GroupsMock* groupsMock = new GroupsMock();
    GroupUsersMock* groupUsersMock = new GroupUsersMock(*groupsMock, *accountMock);
    GroupKeysMock* groupKeysMock = new GroupKeysMock();
    PendingGroupUsersMock* pendingGroupUsersMock = new PendingGroupUsersMock();
    GroupMsgMock* groupMsg = new GroupMsgMock();
    controller.m_accountsManager->m_accounts.reset(accountMock);
    controller.m_groups.reset(groupsMock);
    controller.m_groupUsers.reset(groupUsersMock);
    controller.m_pendingGroupUsers.reset(pendingGroupUsersMock);
    controller.m_groupMessage.reset(groupMsg);
    controller.m_groupKeys.reset(groupKeysMock);

    bcm::Group group;
    groupsMock->generateGroup(group);
    std::string setting;
    std::string settingSig;
    int ownerConfirm = 0;
    std::string settingConfirmSig;
    bcm::Account account = accountMock->m_accounts.begin()->second;
    generateGroupSetting(0,
                         PrivateKeyStore::getInstance().get(account.uid()),
                         setting,
                         settingSig,
                         ownerConfirm,
                         settingConfirmSig);

    CreateGroupBodyV3 req;
    req.name = "";
    req.icon = "";
    req.intro = "";
    req.ownerProof = "ownerProof";
    req.ownerSecretKey = "ownerSecretKey";
    req.groupKeys.encryptVersion = 1;
    //owner
    GroupKeyEntryV0 entry;
    entry.uid = "owner";
    entry.key = "key1";
    entry.deviceId = 0;
    req.groupKeys.keysV0.emplace_back(entry);

    int i = 0;
    for (const auto& item : accountMock->m_accounts) {
        if (item.first != account.uid()) {
            req.members.emplace_back(item.second.uid());
            req.membersGroupInfoSecrets.emplace_back(item.second.uid());
            req.memberProofs.emplace_back(item.second.uid());
            GroupKeyEntryV0 entry;
            entry.uid = item.second.uid();
            entry.key = "key1";
            entry.deviceId = 0;
            req.groupKeys.keysV0.emplace_back(entry);
            if (i == 3) {
                break;
            }
        }
    }
    req.qrCodeSetting = setting;
    req.ownerConfirm = ownerConfirm;
    req.shareSignature = settingSig;
    req.shareAndOwnerConfirmSignature = settingConfirmSig;
    req.encryptedGroupInfoSecret = "";
    req.encryptedEphemeralKey = "encryptedEphemeralKey1";
    req.groupKeysMode = 0;

    // set mock insert group keys
    GroupKeys groupKeys;
    groupKeys.set_version(0);
    groupKeys.set_creator(account.uid());
    groupKeys.set_groupkeys(nlohmann::json(req.groupKeys).dump());
    groupKeys.set_mode(GroupKeys::ONE_FOR_EACH);
    groupKeysMock->setNextInsertGroupKeys(groupKeys);

    HttpContext context;
    context.authResult = account;
    context.requestEntity = req;

    controller.onCreateGroupV3(context);
    REQUIRE(context.response.result() == http::status::ok);
    JsonSerializerImp<CreateGroupBodyV3Resp> jsonSerializerImpCreateGroupBodyV3Resp;
    std::string strResp = "";
    REQUIRE(jsonSerializerImpCreateGroupBodyV3Resp.serialize(context.responseEntity, strResp) == true);
    std::cout << "Create Group V3 Resp: " << strResp << std::endl;
    REQUIRE(strResp != "null");
}

TEST_CASE("testCreateGroupV3")
{
    testCreateGroupV3();
}

void testSendGroupMsgWithRetry()
{
    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    GroupConfig groupConfig;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    AccountMock* accountMock = new AccountMock();
    accountMock->setErrorCode(bcm::dao::ERRORCODE_INTERNAL_ERROR);
    controller.m_accountsManager->m_accounts.reset(accountMock);

    GroupMsgMock* groupMsg = new GroupMsgMock();
    controller.m_groupMessage.reset(groupMsg);
    groupMsg->setErrorCode(bcm::dao::ERRORCODE_INTERNAL_ERROR);

    REQUIRE(controller.sendSwitchGroupKeysWithRetry("uid", 123123, 1111111) == false);
    REQUIRE((groupMsg->m_groupMsgs[123123]).size() == 3);

    REQUIRE(controller.sendGroupKeysUpdateRequestWithRetry("uid", 123124, 0) == false);
    REQUIRE((groupMsg->m_groupMsgs[123124]).size() == 3);

    groupMsg->setErrorCode(bcm::dao::ERRORCODE_SUCCESS);
    REQUIRE(controller.sendSwitchGroupKeysWithRetry("uid", 123123, 1111111) == true);
    REQUIRE(controller.sendGroupKeysUpdateRequestWithRetry("uid", 123124, 0) == true);
}

TEST_CASE("testSendGroupMsgWithRetry")
{
    testSendGroupMsgWithRetry();
}

void testMemberChange()
{
    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);

    // -----|--------------|----------------|---------------
    //     min            max    normalGroupRefreshKeysMax
    //     200            220              240
    // 1. (-,min], power group, change keys every time
    // 2. (min, max], if current is power group, change power group keys, else change normal group keys.
    // 3. (max, normalGroupRefreshKeysMax], normal group, change normal group keys every time
    // 4. (normalGroupRefreshKeysMax, --), normal group, if current is power group, change group keys, else not

    GroupConfig groupConfig;
    groupConfig.powerGroupMin = 200;
    groupConfig.powerGroupMax = 220;
    groupConfig.normalGroupRefreshKeysMax = 240;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    AccountMock* accountMock = new AccountMock();
    controller.m_accountsManager->m_accounts.reset(accountMock);
    GroupMsgMock* groupMsg = new GroupMsgMock();
    GroupKeysMock* groupKeysMock = new GroupKeysMock();
    controller.m_groupMessage.reset(groupMsg);
    controller.m_groupKeys.reset(groupKeysMock);
    uint64_t gid = 123123;

    GroupUpdateGroupKeysRequestBody groupUpdateGroupKeysRequestBody;
    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ONE_FOR_EACH;

    REQUIRE(controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 199) == true);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[gid]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
    gid++;

    controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 200);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[gid]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
    gid++;

    groupKeysMock->setNextGetLastMode(GroupKeys::ONE_FOR_EACH);
    controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 201);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[gid]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
    gid++;

    groupKeysMock->setNextGetLastMode(GroupKeys::ONE_FOR_EACH);
    controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 219);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[gid]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
    gid++;

    groupKeysMock->setNextGetLastMode(GroupKeys::ALL_THE_SAME);
    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ALL_THE_SAME;
    REQUIRE(controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 201) == true);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[gid]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
    gid++;

    groupKeysMock->setNextGetLastMode(GroupKeys::ALL_THE_SAME);
    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ALL_THE_SAME;
    controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 220);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[gid]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
    gid++;

    groupKeysMock->setNextGetLastMode(GroupKeys::ONE_FOR_EACH);
    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ONE_FOR_EACH;
    controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 220);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[gid]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
    gid++;

    groupKeysMock->setNextGetLastMode(GroupKeys::ONE_FOR_EACH);
    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ALL_THE_SAME;
    REQUIRE(controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 221) == true);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[gid]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
    gid++;

    groupKeysMock->setNextGetLastMode(GroupKeys::ALL_THE_SAME);
    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ALL_THE_SAME;
    controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 221);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[gid]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
    gid++;

    groupKeysMock->setNextGetLastMode(GroupKeys::ONE_FOR_EACH);
    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ALL_THE_SAME;
    controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 240);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[gid]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
    gid++;

    groupKeysMock->setNextGetLastMode(GroupKeys::ALL_THE_SAME);
    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ALL_THE_SAME;
    controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 240);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[gid]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
    gid++;

    groupKeysMock->setNextGetLastMode(GroupKeys::ALL_THE_SAME);
    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ALL_THE_SAME;
    controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 241);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 0);
    gid++;

    groupKeysMock->setNextGetLastMode(GroupKeys::ONE_FOR_EACH);
    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ALL_THE_SAME;
    controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 241);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[gid]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
    gid++;

    groupKeysMock->setNextGetLastMode(GroupKeys::ONE_FOR_EACH);
    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ALL_THE_SAME;
    controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 99999);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[gid]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
    gid++;

    groupKeysMock->setNextGetLastMode(GroupKeys::ALL_THE_SAME);
    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ALL_THE_SAME;
    REQUIRE(controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 999999) == true);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 0);
    gid++;

    // test get mode error
    groupKeysMock->setErrorCode(bcm::dao::ERRORCODE_INTERNAL_ERROR);

    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ONE_FOR_EACH;
    controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 210);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[gid]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
    gid++;

    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ALL_THE_SAME;
    controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 9999);
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[gid]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
    gid++;

    // test send msg fail
    groupMsg->setErrorCode(bcm::dao::ERRORCODE_INTERNAL_ERROR);
    REQUIRE(controller.sendGroupKeysUpdateRequestWhenMemberChanges("uid", gid, 199) == false);
    // retry 3 times
    REQUIRE((groupMsg->m_groupMsgs[gid]).size() == 3);
    gid++;
}

TEST_CASE("testMemberChange")
{
    testMemberChange();
}

std::string getGroupKeysByVersion(uint64_t gid, int64_t groupVersion, std::string mode)
{
    std::string groupKeysJson = "{\n"
                                "\t\"gid\": " + std::to_string(gid) + ",\n"
                                "\t\"version\": " + std::to_string(groupVersion) + ",\n"
                               "\t\"group_keys_mode\": " + mode + ",\n"
                               "\t\"group_keys\": {\n"
                               "\t\t\"encrypt_version\": 1,\n"
                               "\t\t\"keys_v0\": [{\n"
                               "\t\t\t\t\"uid\": \"uid1\",\n"
                               "\t\t\t\t\"device_id\": 0,\n"
                               "\t\t\t\t\"key\": \"key1\"\n"
                               "\t\t\t},\n"
                               "\t\t\t{\n"
                               "\t\t\t\t\"uid\": \"uid2\",\n"
                               "\t\t\t\t\"device_id\": 0,\n"
                               "\t\t\t\t\"key\": \"key2\"\n"
                               "\t\t\t},\n"
                               "\t\t\t{\n"
                               "\t\t\t\t\"uid\": \"uid3\",\n"
                               "\t\t\t\t\"device_id\": 0,\n"
                               "\t\t\t\t\"key\": \"key3\"\n"
                               "\t\t\t},\n"
                               "\t\t\t{\n"
                               "\t\t\t\t\"uid\": \"uid" + std::to_string(groupVersion) + "\",\n"
                               "\t\t\t\t\"device_id\": 0,\n"
                               "\t\t\t\t\"key\": \"key4\"\n"
                               "\t\t\t}\n"
                               "\t\t],\n"
                               "\t\t\"keys_v1\": {\n"
                               "\t\t\t\"key\": \"keyv1\"\n"
                               "\t\t}\n"
                               "\t}\n"
                               "}";
    return groupKeysJson;
}

void testUploadAndFetchLatestGroupKeys()
{
    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    AccountMock* accountMock = new AccountMock();
    GroupMsgMock* groupMsg = new GroupMsgMock();
    GroupKeysMock* groupKeysMock = new GroupKeysMock();
    GroupsMock* groupsMock = new GroupsMock();
    GroupUsersMock* groupUsersMock = new GroupUsersMock(*groupsMock, *accountMock);
    bcm::Account account = accountMock->m_accounts.begin()->second;
    auto device = *account.add_devices();
    device.set_id(0);
    account.set_authdeviceid(0);

    GroupConfig groupConfig;
    groupConfig.powerGroupMin = 200;
    groupConfig.powerGroupMax = 220;
    groupConfig.normalGroupRefreshKeysMax = 240;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    controller.m_groups.reset(groupsMock);
    controller.m_groupMessage.reset(groupMsg);
    controller.m_groupKeys.reset(groupKeysMock);
    controller.m_groupUsers.reset(groupUsersMock);

    // create group
    bcm::Group group;
    group.set_version(static_cast<int32_t>(group::GroupVersion::GroupV3));
    groupsMock->createByGid(group, 1001);
    groupsMock->createByGid(group, 1002);
    groupsMock->createByGid(group, 1003);
    groupsMock->createByGid(group, 1004);

    bcm::GroupUser userUid3;
    userUid3.set_uid("uid3");
    userUid3.set_gid(1001);
    groupUsersMock->insert(userUid3);
    userUid3.set_gid(1002);
    groupUsersMock->insert(userUid3);
    userUid3.set_gid(1003);
    groupUsersMock->insert(userUid3);
    userUid3.set_gid(1004);
    groupUsersMock->insert(userUid3);

    HttpContext context;
    account.set_uid("uid3");
    context.authResult = account;

    int64_t groupVersion1 = 111111;
    int64_t groupVersion2 = 222222;
    int64_t groupVersion3 = 333333;
    std::string groupKeysJsonV1 = getGroupKeysByVersion(1001, groupVersion1, "0");
    std::string groupKeysJsonV2 = getGroupKeysByVersion(1002, groupVersion2, "1");
    std::string groupKeysJsonV3 = getGroupKeysByVersion(1004, groupVersion3, "0");

    JsonSerializerImp<UploadGroupKeysRequest> jsonSerializerImp;
    REQUIRE(jsonSerializerImp.deserialize(groupKeysJsonV1, context.requestEntity) == true);
    controller.onUploadGroupKeysV3(context);
    REQUIRE(context.response.result() == http::status::no_content);

    REQUIRE(jsonSerializerImp.deserialize(groupKeysJsonV2, context.requestEntity) == true);
    controller.onUploadGroupKeysV3(context);
    REQUIRE(context.response.result() == http::status::no_content);

    REQUIRE(jsonSerializerImp.deserialize(groupKeysJsonV3, context.requestEntity) == true);
    controller.onUploadGroupKeysV3(context);
    REQUIRE(context.response.result() == http::status::no_content);
    REQUIRE(groupKeysMock->m_groupKeys.size() == 3);

    std::string fetchRequestV1 = "{\"gids\":[1001,1002,1003,1004]}";
    JsonSerializerImp<FetchLatestGroupKeysRequest> jsonSerializerImpFetchGroup;
    REQUIRE(jsonSerializerImpFetchGroup.deserialize(fetchRequestV1, context.requestEntity) == true);
    controller.onFetchLatestGroupKeysV3(context);
    REQUIRE(context.response.result() == http::status::ok);

    JsonSerializerImp<FetchLatestGroupKeysResponse> jsonSerializerImpFetchLatestGroupKeysResponse;
    std::string strResp = "";
    REQUIRE(jsonSerializerImpFetchLatestGroupKeysResponse.serialize(context.responseEntity, strResp) == true);
    REQUIRE(strResp != "null");
    std::cout << "Fetch latest key Resp: " << strResp << std::endl;

    FetchLatestGroupKeysResponse fetchLatestGroupKeysResponse = boost::any_cast<FetchLatestGroupKeysResponse>(context.responseEntity);
    REQUIRE(fetchLatestGroupKeysResponse.keys.size() == 3);

    for (auto& e : fetchLatestGroupKeysResponse.keys) {
        if (e.version == groupVersion1) {
            REQUIRE(e.gid == 1001);
            REQUIRE(e.encryptVersion == 1);
            REQUIRE(e.groupKeysMode == 0);
            REQUIRE(e.keysV0.uid == "uid3");
            REQUIRE(e.keysV0.key == "key3");
        } else if (e.version == groupVersion2) {
            REQUIRE(e.gid == 1002);
            REQUIRE(e.encryptVersion == 1);
            REQUIRE(e.groupKeysMode == 1);
            REQUIRE(e.keysV1.key == "keyv1");
        } else if (e.version == groupVersion3) {
            REQUIRE(e.gid == 1004);
            REQUIRE(e.encryptVersion == 1);
            REQUIRE(e.groupKeysMode == 0);
            REQUIRE(e.keysV0.uid == "uid3");
            REQUIRE(e.keysV0.key == "key3");
        } else {
            REQUIRE(false);
        }
    }

    std::string fetchRequestV2 = "{\"gids\":[1001,1002,1003,1004,1005]}";
    REQUIRE(jsonSerializerImpFetchGroup.deserialize(fetchRequestV2, context.requestEntity) == true);
    controller.onFetchLatestGroupKeysV3(context);
    REQUIRE(context.response.result() == http::status::forbidden);

    // test empty result
    userUid3.set_uid("uid3");
    userUid3.set_gid(10081);
    groupUsersMock->insert(userUid3);
    userUid3.set_gid(10082);
    groupUsersMock->insert(userUid3);
    userUid3.set_gid(10083);
    groupUsersMock->insert(userUid3);
    std::string fetchRequestV3 = "{\"gids\":[10081,10082,10083]}";
    REQUIRE(jsonSerializerImpFetchGroup.deserialize(fetchRequestV3, context.requestEntity) == true);
    controller.onFetchLatestGroupKeysV3(context);
    REQUIRE(jsonSerializerImpFetchLatestGroupKeysResponse.serialize(context.responseEntity, strResp) == true);
    REQUIRE(strResp != "null");
    std::cout << "Fetch latest key with group not found, Resp: " << strResp << std::endl;
}

TEST_CASE("testUploadAndFetchLatestGroupKeys")
{
    testUploadAndFetchLatestGroupKeys();
}

void testUploadAndFetchGroupKeys()
{
    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    AccountMock* accountMock = new AccountMock();
    GroupMsgMock* groupMsg = new GroupMsgMock();
    GroupKeysMock* groupKeysMock = new GroupKeysMock();
    GroupsMock* groupsMock = new GroupsMock();
    GroupUsersMock* groupUsersMock = new GroupUsersMock(*groupsMock, *accountMock);
    bcm::Account account = accountMock->m_accounts.begin()->second;
    auto device = *account.add_devices();
    device.set_id(0);
    account.set_authdeviceid(0);

    // create group
    bcm::Group group;
    group.set_version(static_cast<int32_t>(group::GroupVersion::GroupV3));
    groupsMock->createByGid(group, 123123);

    GroupConfig groupConfig;
    groupConfig.powerGroupMin = 200;
    groupConfig.powerGroupMax = 220;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    controller.m_groups.reset(groupsMock);
    controller.m_groupMessage.reset(groupMsg);
    controller.m_groupKeys.reset(groupKeysMock);
    controller.m_groupUsers.reset(groupUsersMock);

    int64_t groupVersion1 = 111111;
    int64_t groupVersion2 = 222222;
    int64_t groupVersion3 = 333333;
    std::string groupKeysJsonV1 = getGroupKeysByVersion(123123, groupVersion1, "0");
    std::string groupKeysJsonV2 = getGroupKeysByVersion(123123, groupVersion2, "1");
    std::string groupKeysJsonV3 = getGroupKeysByVersion(123123,groupVersion3, "0");

    HttpContext context;
    account.set_uid("uid3");
    context.authResult = account;

    bcm::GroupUser userUid3;
    userUid3.set_gid(123123);
    userUid3.set_uid("uid3");
    groupUsersMock->insert(userUid3);

    JsonSerializerImp<UploadGroupKeysRequest> jsonSerializerImp;
    REQUIRE(jsonSerializerImp.deserialize(groupKeysJsonV1, context.requestEntity) == true);
    controller.onUploadGroupKeysV3(context);
    REQUIRE(context.response.result() == http::status::no_content);

    REQUIRE(jsonSerializerImp.deserialize(groupKeysJsonV2, context.requestEntity) == true);
    controller.onUploadGroupKeysV3(context);
    REQUIRE(context.response.result() == http::status::no_content);

    REQUIRE(jsonSerializerImp.deserialize(groupKeysJsonV3, context.requestEntity) == true);
    controller.onUploadGroupKeysV3(context);
    REQUIRE(context.response.result() == http::status::no_content);
    REQUIRE(groupKeysMock->m_groupKeys.size() == 3);

    groupKeysMock->setErrorCode(bcm::dao::ERRORCODE_CAS_FAIL);
    controller.onUploadGroupKeysV3(context);
    REQUIRE(context.response.result() == http::status::conflict);
    groupKeysMock->setErrorCode(bcm::dao::ERRORCODE_SUCCESS);

    // test upload not group member
    account.set_uid("uid4");
    context.authResult = account;
    REQUIRE(jsonSerializerImp.deserialize(groupKeysJsonV3, context.requestEntity) == true);
    controller.onUploadGroupKeysV3(context);
    REQUIRE(context.response.result() == http::status::forbidden);

    // test fetch
    std::string fetchRequestV1 = "{\"gid\": 123123,\"versions\":[111111,1,2,3,222222]}";
    // want to get 2 group keys
    account.set_uid("uid3");
    context.authResult = account;
    JsonSerializerImp<FetchGroupKeysRequest> jsonSerializerImpFetchGroup;
    REQUIRE(jsonSerializerImpFetchGroup.deserialize(fetchRequestV1, context.requestEntity) == true);
    controller.onFetchGroupKeysV3(context);
    REQUIRE(context.response.result() == http::status::ok);

    JsonSerializerImp<FetchGroupKeysResponse> jsonSerializerImpFetchGroupKeysResponse;
    std::string strResp = "";
    REQUIRE(jsonSerializerImpFetchGroupKeysResponse.serialize(context.responseEntity, strResp) == true);
    REQUIRE(strResp != "null");
    std::cout << "Fetch key Resp: " << strResp << std::endl;

    FetchGroupKeysResponse fetchGroupKeysResponse = boost::any_cast<FetchGroupKeysResponse>(context.responseEntity);
    REQUIRE(fetchGroupKeysResponse.gid == 123123);
    REQUIRE(fetchGroupKeysResponse.keys.size() == 2);

    for (auto& e : fetchGroupKeysResponse.keys) {
        if (e.version == groupVersion1) {
            REQUIRE(e.encryptVersion == 1);
            REQUIRE(e.groupKeysMode == 0);
            REQUIRE(e.keysV0.uid == "uid3");
            REQUIRE(e.keysV0.key == "key3");
        } else if (e.version == groupVersion2) {
            REQUIRE(e.encryptVersion == 1);
            REQUIRE(e.groupKeysMode == 1);
            REQUIRE(e.keysV1.key == "keyv1");
        } else {
            REQUIRE(false);
        }
    }

    // test uid not in group keys
    std::string fetchRequestV2 = "{\"gid\":123123,\"versions\":[111111,1,2,333333]}";
    // want to get 1 group keys
    account.set_uid("uid111111");
    bcm::GroupUser userUid111111;
    userUid111111.set_gid(123123);
    userUid111111.set_uid("uid111111");
    groupUsersMock->insert(userUid111111);

    context.authResult = account;
    REQUIRE(jsonSerializerImpFetchGroup.deserialize(fetchRequestV2, context.requestEntity) == true);
    controller.onFetchGroupKeysV3(context);
    REQUIRE(context.response.result() == http::status::ok);
    FetchGroupKeysResponse fetchGroupKeysResponse1 = boost::any_cast<FetchGroupKeysResponse>(context.responseEntity);
    REQUIRE(fetchGroupKeysResponse1.gid == 123123);
    REQUIRE(fetchGroupKeysResponse1.keys.size() == 1);
    for (auto& e : fetchGroupKeysResponse1.keys) {
        if (e.version == groupVersion1) {
            REQUIRE(e.encryptVersion == 1);
            REQUIRE(e.groupKeysMode == 0);
            REQUIRE(e.keysV0.uid == "uid111111");
            REQUIRE(e.keysV0.key == "key4");
        } else {
            REQUIRE(false);
        }
    }

    // test fetch not exists version
    std::string fetchRequestV3 = "{\"gid\":123123,\"versions\":[88888,99999]}";
    groupKeysMock->setErrorCode(bcm::dao::ErrorCode::ERRORCODE_NO_SUCH_DATA);
    account.set_uid("uid3");
    context.authResult = account;
    REQUIRE(jsonSerializerImpFetchGroup.deserialize(fetchRequestV3, context.requestEntity) == true);
    controller.onFetchGroupKeysV3(context);
    REQUIRE(context.response.result() == http::status::ok);
    FetchGroupKeysResponse fetchGroupKeysResponse3 = boost::any_cast<FetchGroupKeysResponse>(context.responseEntity);
    REQUIRE(fetchGroupKeysResponse3.gid == 123123);
    REQUIRE(fetchGroupKeysResponse3.keys.size() == 0);

    strResp = "";
    REQUIRE(jsonSerializerImpFetchGroupKeysResponse.serialize(context.responseEntity, strResp) == true);
    REQUIRE(strResp != "null");
    std::cout << "onFetchGroupKeysV3, want to fetch not exists version, Resp: " << strResp << std::endl;

    // test upload gid is not V3
    uint64_t gid = 1111222;
    bcm::Group groupV0;
    groupV0.set_version(static_cast<int32_t>(group::GroupVersion::GroupV0));
    groupsMock->createByGid(groupV0, gid);
    userUid3.set_gid(gid);
    userUid3.set_uid("uid3");
    groupUsersMock->insert(userUid3);

    std::string groupKeysJsonV4 = getGroupKeysByVersion(gid, 4444444, "0");
    REQUIRE(jsonSerializerImp.deserialize(groupKeysJsonV4, context.requestEntity) == true);
    controller.onUploadGroupKeysV3(context);
    REQUIRE(context.response.result() == http::status::bad_request);

}

TEST_CASE("testUploadAndFetchGroupKeys")
{
    testUploadAndFetchGroupKeys();
}

void testFireGroupKeysUpdateV3()
{
    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    AccountMock* accountMock = new AccountMock();
    GroupMsgMock* groupMsg = new GroupMsgMock();
    GroupKeysMock* groupKeysMock = new GroupKeysMock();
    GroupsMock* groupsMock = new GroupsMock();
    GroupUsersMock* groupUsersMock = new GroupUsersMock(*groupsMock, *accountMock);
    bcm::Account account = accountMock->m_accounts.begin()->second;

    GroupConfig groupConfig;
    groupConfig.powerGroupMin = 200;
    groupConfig.powerGroupMax = 220;
    groupConfig.normalGroupRefreshKeysMax = 240;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    controller.m_groups.reset(groupsMock);
    controller.m_groupMessage.reset(groupMsg);
    controller.m_groupKeys.reset(groupKeysMock);
    controller.m_groupUsers.reset(groupUsersMock);
    controller.m_accountsManager->m_accounts.reset(accountMock);

    // create group
    bcm::Group group;
    group.set_version(static_cast<int32_t>(group::GroupVersion::GroupV3));
    groupsMock->createByGid(group, 1001);
    groupsMock->createByGid(group, 1002);
    groupsMock->createByGid(group, 1004);

    bcm::GroupUser userUid3;
    userUid3.set_uid("uid3");
    userUid3.set_gid(1001);
    groupUsersMock->insert(userUid3);
    for (int i=0;i<210;i++) {
        userUid3.set_uid("uid" + std::to_string(i));
        userUid3.set_gid(1002);
        groupUsersMock->insert(userUid3);
    }
    for (int i=0;i<250;i++) {
        userUid3.set_uid("uid" + std::to_string(i));
        userUid3.set_gid(1004);
        groupUsersMock->insert(userUid3);
    }

    int64_t groupVersion1 = 111111;
    int64_t groupVersion3 = 333333;
    std::string groupKeysJsonV1 = getGroupKeysByVersion(1001, groupVersion1, "0");
    std::string groupKeysJsonV3 = getGroupKeysByVersion(1004, groupVersion3, "0");

    HttpContext context;
    account.set_uid("uid3");
    context.authResult = account;

    JsonSerializerImp<UploadGroupKeysRequest> jsonSerializerImpUpload;
    REQUIRE(jsonSerializerImpUpload.deserialize(groupKeysJsonV1, context.requestEntity) == true);
    controller.onUploadGroupKeysV3(context);
    REQUIRE(context.response.result() == http::status::no_content);
    groupMsg->m_groupMsgs[1001].clear();

    REQUIRE(jsonSerializerImpUpload.deserialize(groupKeysJsonV3, context.requestEntity) == true);
    controller.onUploadGroupKeysV3(context);
    REQUIRE(context.response.result() == http::status::no_content);
    groupMsg->m_groupMsgs[1004].clear();
    REQUIRE(groupKeysMock->m_groupKeys.size() == 2);

    std::string fireRequestV1 = "{\"gids\":[1001,1002,1003]}";
    JsonSerializerImp<FireGroupKeysUpdateRequest> jsonSerializerImp;
    REQUIRE(jsonSerializerImp.deserialize(fireRequestV1, context.requestEntity) == true);
    controller.fireGroupKeysUpdateV3(context);
    REQUIRE(context.response.result() == http::status::forbidden);

    std::string fireRequestV2 = "{\"gids\":[1001,1002,1004]}";
    REQUIRE(jsonSerializerImp.deserialize(fireRequestV2, context.requestEntity) == true);
    controller.fireGroupKeysUpdateV3(context);
    REQUIRE(context.response.result() == http::status::ok);

    JsonSerializerImp<FireGroupKeysUpdateResponse> jsonSerializerImpFireGroupKeysUpdateResponse;
    std::string strResp = "";
    REQUIRE(jsonSerializerImpFireGroupKeysUpdateResponse.serialize(context.responseEntity, strResp) == true);
    REQUIRE(strResp != "null");
    std::cout << "FireGroupKeysUpdateV3 Resp: " << strResp << std::endl;

    FireGroupKeysUpdateResponse fireGroupKeysUpdateResponse = boost::any_cast<FireGroupKeysUpdateResponse>(context.responseEntity);
    REQUIRE(fireGroupKeysUpdateResponse.success.size()== 3);
    for (auto& s : fireGroupKeysUpdateResponse.success) {
        REQUIRE( (s == 1001 || s == 1002 || s == 1004) );
    }

    GroupUpdateGroupKeysRequestBody groupUpdateGroupKeysRequestBody;
    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ONE_FOR_EACH;
    REQUIRE((groupMsg->m_groupMsgs[1001]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[1001]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());

    REQUIRE((groupMsg->m_groupMsgs[1002]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[1002]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());

    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ALL_THE_SAME;
    REQUIRE((groupMsg->m_groupMsgs[1004]).size() == 1);
    REQUIRE(((groupMsg->m_groupMsgs[1004]).begin()->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
}

TEST_CASE("testFireGroupKeysUpdateV3")
{
    testFireGroupKeysUpdateV3();
}

void testLeaveGroupV3()
{
    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    AccountMock* accountMock = new AccountMock();
    GroupMsgMock* groupMsg = new GroupMsgMock();
    GroupKeysMock* groupKeysMock = new GroupKeysMock();
    GroupsMock* groupsMock = new GroupsMock();
    GroupUsersMock* groupUsersMock = new GroupUsersMock(*groupsMock, *accountMock);
    bcm::Account account = accountMock->m_accounts.begin()->second;

    GroupConfig groupConfig;
    groupConfig.powerGroupMin = 200;
    groupConfig.powerGroupMax = 220;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    controller.m_groups.reset(groupsMock);
    controller.m_groupMessage.reset(groupMsg);
    controller.m_groupKeys.reset(groupKeysMock);
    controller.m_groupUsers.reset(groupUsersMock);
    controller.m_accountsManager->m_accounts.reset(accountMock);

    // create group
    bcm::Group group;
    group.set_version(static_cast<int32_t>(group::GroupVersion::GroupV3));
    groupsMock->createByGid(group, 1001);

    for (int i=0; i<10; ++i) {
        bcm::GroupUser user;
        user.set_uid("uid" + std::to_string(i));
        user.set_gid(1001);
        if (i == 0) {
            user.set_role(GroupUser::ROLE_OWNER);
        } else {
            user.set_role(GroupUser::ROLE_MEMBER);
        }
        groupUsersMock->insert(user);
    }

    LeaveGroupBody leaveGroupBody;
    leaveGroupBody.gid = 1001;

    HttpContext context;
    account.set_uid("uid3");
    context.authResult = account;
    context.requestEntity = leaveGroupBody;

    controller.onLeaveGroupV3(context);
    REQUIRE(context.response.result() == http::status::no_content);
    GroupUpdateGroupKeysRequestBody groupUpdateGroupKeysRequestBody;
    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ONE_FOR_EACH;

    REQUIRE((groupMsg->m_groupMsgs[1001]).size() == 2);
    auto iter = groupMsg->m_groupMsgs[1001].begin();
    iter++;
    REQUIRE((iter->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());

}

TEST_CASE("testLeaveGroupV3")
{
    testLeaveGroupV3();
}

void testKickGroupV3()
{
    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    AccountMock* accountMock = new AccountMock();
    GroupMsgMock* groupMsg = new GroupMsgMock();
    GroupKeysMock* groupKeysMock = new GroupKeysMock();
    GroupsMock* groupsMock = new GroupsMock();
    GroupUsersMock* groupUsersMock = new GroupUsersMock(*groupsMock, *accountMock);
    bcm::Account account = accountMock->m_accounts.begin()->second;

    GroupConfig groupConfig;
    groupConfig.powerGroupMin = 200;
    groupConfig.powerGroupMax = 220;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    controller.m_groups.reset(groupsMock);
    controller.m_groupMessage.reset(groupMsg);
    controller.m_groupKeys.reset(groupKeysMock);
    controller.m_groupUsers.reset(groupUsersMock);
    controller.m_accountsManager->m_accounts.reset(accountMock);

    // create group
    bcm::Group group;
    group.set_version(static_cast<int32_t>(group::GroupVersion::GroupV3));
    groupsMock->createByGid(group, 1001);

    for (int i=0; i<5; ++i) {
        bcm::GroupUser user;
        user.set_uid("uid" + std::to_string(i));
        user.set_gid(1001);
        if (i == 0) {
            user.set_role(GroupUser::ROLE_OWNER);
        } else {
            user.set_role(GroupUser::ROLE_MEMBER);
        }
        groupUsersMock->insert(user);
    }

    KickGroupMemberBody kickGroupMemberBody;
    kickGroupMemberBody.gid = 1001;
    kickGroupMemberBody.members.emplace_back("uid1");
    kickGroupMemberBody.members.emplace_back("uid2");
    kickGroupMemberBody.members.emplace_back("uid3");
    kickGroupMemberBody.members.emplace_back("uid4");

    HttpContext context;
    account.set_uid("uid0");
    context.authResult = account;
    context.requestEntity = kickGroupMemberBody;

    controller.onKickGroupMemberV3(context);
    REQUIRE(context.response.result() == http::status::no_content);
    GroupUpdateGroupKeysRequestBody groupUpdateGroupKeysRequestBody;
    groupUpdateGroupKeysRequestBody.groupKeysMode = GroupKeys::ONE_FOR_EACH;

    REQUIRE((groupMsg->m_groupMsgs[1001]).size() == 2);
    auto iter = groupMsg->m_groupMsgs[1001].begin();
    iter++;
    REQUIRE((iter->second).text() == nlohmann::json(groupUpdateGroupKeysRequestBody).dump());
}

TEST_CASE("testKickGroupV3")
{
    testKickGroupV3();
}

void testOnDhKeys()
{
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    GroupConfig groupConfig;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    AccountMock* accountMock = new AccountMock();
    controller.m_accountsManager->m_accounts.reset(accountMock);

    bcm::DhKeysRequest request;
    bcm::HttpContext context;
    bool first = true;
    for (auto& acc : accountMock->m_accounts) {
        if (first) {
            first = false;
            acc.second.set_authdeviceid(1);
            context.authResult = acc.second;
        }
        request.uids.emplace(acc.second.uid());
    }
    context.requestEntity = request;
    controller.onDhKeysV3(context);
    REQUIRE(context.response.result_int() == static_cast<unsigned>(http::status::ok));
    //TODO check data
}

TEST_CASE("onDhKeys")
{
    initMetricsClientGroup();
    testOnDhKeys();
}

TEST_CASE("onGroupJoinRequstV3")
{
    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    GroupConfig groupConfig;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    AccountMock* accountMock = new AccountMock();
    GroupsMock* groupsMock = new GroupsMock();
    GroupUsersMock* groupUsersMock = new GroupUsersMock(*groupsMock, *accountMock);
    PendingGroupUsersMock* pendingGroupUsersMock = new PendingGroupUsersMock();
    GroupMsgMock* groupMsg = new GroupMsgMock();
    GroupKeysMock* groupKeysMock = new GroupKeysMock();
    QrCodeGroupUsersMock* qrCodeGroupUsers = new QrCodeGroupUsersMock();
    controller.m_accountsManager->m_accounts.reset(accountMock);
    controller.m_groups.reset(groupsMock);
    controller.m_groupUsers.reset(groupUsersMock);
    controller.m_pendingGroupUsers.reset(pendingGroupUsersMock);
    controller.m_groupMessage.reset(groupMsg);
    controller.m_qrCodeGroupUsers.reset(qrCodeGroupUsers);
    controller.m_groupKeys.reset(groupKeysMock);

    HttpContext context;
    bcm::GroupJoinRequestV3 req;
    bcm::Group group = groupsMock->m_groups.begin()->second;
    req.gid = group.gid();
    std::string ownerUid;
    REQUIRE(groupUsersMock->getGroupOwner(req.gid, ownerUid) == bcm::dao::ERRORCODE_SUCCESS);
    bcm::GroupUser owner;
    REQUIRE(groupUsersMock->getMember(req.gid, ownerUid, owner) == bcm::dao::ERRORCODE_SUCCESS);

    std::string setting;
    std::string settingSig;
    int ownerConfirm = 0;
    std::string settingConfirmSig;
    generateGroupSetting(req.gid,
                         PrivateKeyStore::getInstance().get(ownerUid),
                         setting,
                         settingSig,
                         ownerConfirm,
                         settingConfirmSig);
    groupsMock->m_groups[req.gid].set_shareqrcodesetting(setting);
    groupsMock->m_groups[req.gid].set_ownerconfirm(ownerConfirm);
    groupsMock->m_groups[req.gid].set_sharesignature(settingSig);
    groupsMock->m_groups[req.gid].set_shareandownerconfirmsignature(settingConfirmSig);

    std::ostringstream oss;
    oss << "code" << bcm::Base64::decode(settingSig);
    bcm::Account account;
    auto it = groupUsersMock->m_groupUsers.find(req.gid);
    REQUIRE(it != groupUsersMock->m_groupUsers.end());
    for (const auto& item : it->second) {
        if (item.second.uid() != ownerUid) {
            REQUIRE(accountMock->get(item.second.uid(), account) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
            break;
        }
    }
    req.qrCode = bcm::Base64::encode("code");
    req.qrCodeToken = settingSig;
    calculateSignature(PrivateKeyStore::getInstance().get(account.uid()), oss.str(), req.signature);
    req.comment = "";

    context.authResult = account;
    context.requestEntity = req;
    bcm::GroupUser gu = groupUsersMock->m_groupUsers[req.gid].at(account.uid());
    groupUsersMock->m_groupUsers[req.gid].erase(account.uid());

    // happpy path when ownerConfirm=0
    groupUsersMock->m_groupUsers[req.gid].erase(account.uid());
    controller.onGroupJoinRequstV3(context);
    REQUIRE(context.response.result() == http::status::ok);
    bcm::QrCodeGroupUser user;
    REQUIRE(qrCodeGroupUsers->get(req.gid, account.uid(), user) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);

    // happy path when ownerConfirm = 1
    ownerConfirm = 1;
    generateGroupSetting(req.gid,
                         PrivateKeyStore::getInstance().get(ownerUid),
                         setting,
                         settingSig,
                         ownerConfirm,
                         settingConfirmSig);
    groupsMock->m_groups[req.gid].set_shareqrcodesetting(setting);
    groupsMock->m_groups[req.gid].set_ownerconfirm(ownerConfirm);
    groupsMock->m_groups[req.gid].set_sharesignature(settingSig);
    groupsMock->m_groups[req.gid].set_shareandownerconfirmsignature(settingConfirmSig);

    oss.str("");
    oss << "code" << bcm::Base64::decode(settingSig);
    req.qrCode = bcm::Base64::encode("code");
    req.qrCodeToken = settingSig;
    calculateSignature(PrivateKeyStore::getInstance().get(account.uid()), oss.str(), req.signature);
    req.comment = "";

    context.requestEntity = req;
    groupUsersMock->m_groupUsers[req.gid].erase(account.uid());
    controller.onGroupJoinRequstV3(context);
    REQUIRE(context.response.result() == http::status::ok);
}

TEST_CASE("onAddMeRequstV3")
{
    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    GroupConfig groupConfig;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    AccountMock* accountMock = new AccountMock();
    GroupsMock* groupsMock = new GroupsMock();
    GroupUsersMock* groupUsersMock = new GroupUsersMock(*groupsMock, *accountMock);
    PendingGroupUsersMock* pendingGroupUsersMock = new PendingGroupUsersMock();
    GroupMsgMock* groupMsg = new GroupMsgMock();
    GroupKeysMock* groupKeysMock = new GroupKeysMock();
    QrCodeGroupUsersMock* qrCodeGroupUsers = new QrCodeGroupUsersMock();
    controller.m_accountsManager->m_accounts.reset(accountMock);
    controller.m_groups.reset(groupsMock);
    controller.m_groupUsers.reset(groupUsersMock);
    controller.m_pendingGroupUsers.reset(pendingGroupUsersMock);
    controller.m_groupMessage.reset(groupMsg);
    controller.m_qrCodeGroupUsers.reset(qrCodeGroupUsers);
    controller.m_groupKeys.reset(groupKeysMock);

    bcm::AddMeRequestV3 request;
    HttpContext context;

    bcm::Group& group = groupsMock->m_groups.begin()->second;
    bcm::Account& account = accountMock->m_accounts.begin()->second;
    groupUsersMock->m_groupUsers[group.gid()].erase(account.uid());
    bcm::QrCodeGroupUser user;
    user.set_gid(group.gid());
    user.set_uid(account.uid());
    qrCodeGroupUsers->set(user, 60);
    request.gid = group.gid();
    request.groupInfoSecret = "groupInfoSecret";
    request.proof = "proof";

    context.authResult = account;
    context.requestEntity = request;
    controller.onAddMeRequstV3(context);
    REQUIRE(context.response.result() == http::status::ok);
}

TEST_CASE("onReviewJoinRequestV3")
{
    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    GroupConfig groupConfig;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    AccountMock* accountMock = new AccountMock();
    GroupsMock* groupsMock = new GroupsMock();
    GroupUsersMock* groupUsersMock = new GroupUsersMock(*groupsMock, *accountMock);
    PendingGroupUsersMock* pendingGroupUsersMock = new PendingGroupUsersMock();
    GroupMsgMock* groupMsg = new GroupMsgMock();
    GroupKeysMock* groupKeysMock = new GroupKeysMock();
    QrCodeGroupUsersMock* qrCodeGroupUsers = new QrCodeGroupUsersMock();
    controller.m_accountsManager->m_accounts.reset(accountMock);
    controller.m_groups.reset(groupsMock);
    controller.m_groupUsers.reset(groupUsersMock);
    controller.m_pendingGroupUsers.reset(pendingGroupUsersMock);
    controller.m_groupMessage.reset(groupMsg);
    controller.m_qrCodeGroupUsers.reset(qrCodeGroupUsers);
    controller.m_groupKeys.reset(groupKeysMock);

    bcm::Group& group = groupsMock->m_groups.begin()->second;
    bcm::Account owner;
    REQUIRE(bcm::dao::ErrorCode::ERRORCODE_SUCCESS == groupUsersMock->getOwner(group.gid(), owner));
    bcm::Account account;
    for (const auto& item : accountMock->m_accounts) {
        if (item.first != owner.uid()) {
            account = item.second;
            break;
        }
    }
    groupUsersMock->m_groupUsers[group.gid()].erase(account.uid());

    std::string setting;
    std::string settingSig;
    int ownerConfirm = 1;
    std::string settingConfirmSig;
    generateGroupSetting(group.gid(),
                         PrivateKeyStore::getInstance().get(owner.uid()),
                         setting,
                         settingSig,
                         ownerConfirm,
                         settingConfirmSig);
    groupsMock->m_groups[group.gid()].set_shareqrcodesetting(setting);
    groupsMock->m_groups[group.gid()].set_ownerconfirm(ownerConfirm);
    groupsMock->m_groups[group.gid()].set_sharesignature(settingSig);
    groupsMock->m_groups[group.gid()].set_shareandownerconfirmsignature(settingConfirmSig);

    HttpContext context;
    ReviewJoinResultRequestV3 request;

    request.gid = group.gid();
    ReviewJoinResultV3 r;
    r.uid = account.uid();
    r.accepted = true;
    r.groupInfoSecret = "groupInfoSecret";
    r.inviter = "";
    r.proof = "proof";
    request.list.emplace_back(r);

    context.authResult = owner;
    context.requestEntity = request;

    controller.onReviewJoinRequestV3(context);
    REQUIRE(context.response.result() == http::status::ok);
}

TEST_CASE("onPrepareKeyUpdateV3")
{
    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    GroupConfig groupConfig;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    AccountMock* accountMock = new AccountMock();
    GroupsMock* groupsMock = new GroupsMock();
    GroupUsersMock* groupUsersMock = new GroupUsersMock(*groupsMock, *accountMock);
    PendingGroupUsersMock* pendingGroupUsersMock = new PendingGroupUsersMock();
    GroupMsgMock* groupMsg = new GroupMsgMock();
    GroupKeysMock* groupKeysMock = new GroupKeysMock();
    QrCodeGroupUsersMock* qrCodeGroupUsers = new QrCodeGroupUsersMock();
    GroupMsgServiceMock* groupMsgServiceMock = new GroupMsgServiceMock(bcm::RedisConfig(), dispatchManager, bcm::NoiseConfig());

    controller.m_accountsManager->m_accounts.reset(accountMock);
    controller.m_groups.reset(groupsMock);
    controller.m_groupUsers.reset(groupUsersMock);
    controller.m_pendingGroupUsers.reset(pendingGroupUsersMock);
    controller.m_groupMessage.reset(groupMsg);
    controller.m_qrCodeGroupUsers.reset(qrCodeGroupUsers);
    controller.m_groupKeys.reset(groupKeysMock);
    controller.m_groupMsgService.reset(groupMsgServiceMock);

    bcm::Group& group = groupsMock->m_groups.begin()->second;
    bcm::Account owner;
    REQUIRE(bcm::dao::ErrorCode::ERRORCODE_SUCCESS == groupUsersMock->getOwner(group.gid(), owner));
    bcm::Account account;
    for (const auto& item : accountMock->m_accounts) {
        if (item.first != owner.uid()) {
            account = item.second;
            break;
        }
    }

    std::set<std::string> uids;
    auto it = groupUsersMock->m_groupUsers.find(group.gid());
    REQUIRE(it != groupUsersMock->m_groupUsers.end());
    for (const auto& item : it->second) {
        uids.emplace(item.first);
    }
    accountMock->setKeysUids(uids);
    
    PrepareKeyUpdateRequestV3 request;
    HttpContext context;
    request.gid = group.gid();
    request.version = 100;
    request.mode = bcm::GroupKeys::ONE_FOR_EACH;
    context.authResult = owner;
    context.requestEntity = request;

    groupMsgServiceMock->setLocalOnlineGroupMembers(request.gid, {owner.uid()});
    controller.onPrepareKeyUpdateV3(context);
    REQUIRE(context.response.result() == http::status::ok);
}

TEST_CASE("onInviteGroupMemberV3")
{
    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    GroupConfig groupConfig;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    AccountMock* accountMock = new AccountMock();
    GroupsMock* groupsMock = new GroupsMock();
    GroupUsersMock* groupUsersMock = new GroupUsersMock(*groupsMock, *accountMock);
    PendingGroupUsersMock* pendingGroupUsersMock = new PendingGroupUsersMock();
    GroupMsgMock* groupMsg = new GroupMsgMock();
    GroupKeysMock* groupKeysMock = new GroupKeysMock();
    QrCodeGroupUsersMock* qrCodeGroupUsers = new QrCodeGroupUsersMock();
    GroupMsgServiceMock* groupMsgServiceMock = new GroupMsgServiceMock(bcm::RedisConfig(), dispatchManager, bcm::NoiseConfig());

    controller.m_accountsManager->m_accounts.reset(accountMock);
    controller.m_groups.reset(groupsMock);
    controller.m_groupUsers.reset(groupUsersMock);
    controller.m_pendingGroupUsers.reset(pendingGroupUsersMock);
    controller.m_groupMessage.reset(groupMsg);
    controller.m_qrCodeGroupUsers.reset(qrCodeGroupUsers);
    controller.m_groupKeys.reset(groupKeysMock);
    controller.m_groupMsgService.reset(groupMsgServiceMock);

    bcm::Group& group = groupsMock->m_groups.begin()->second;
    
    bcm::Account owner;
    REQUIRE(bcm::dao::ErrorCode::ERRORCODE_SUCCESS == groupUsersMock->getOwner(group.gid(), owner));
    std::string setting;
    std::string settingSig;
    int ownerConfirm = 0;
    std::string settingConfirmSig;
    generateGroupSetting(group.gid(),
                         PrivateKeyStore::getInstance().get(owner.uid()),
                         setting,
                         settingSig,
                         ownerConfirm,
                         settingConfirmSig);
    group.set_shareqrcodesetting(setting);
    group.set_ownerconfirm(ownerConfirm);
    group.set_sharesignature(settingSig);
    group.set_shareandownerconfirmsignature(settingConfirmSig);

    bcm::Account account;
    for (const auto& item : accountMock->m_accounts) {
        if (item.first != owner.uid()) {
            account = item.second;
            break;
        }
    }
    auto itGroup = groupUsersMock->m_groupUsers.find(group.gid());
    REQUIRE(itGroup != groupUsersMock->m_groupUsers.end());
    itGroup->second.erase(account.uid());

    
    
    InviteGroupMemberRequestV3 request;
    HttpContext context;
    request.gid = group.gid();
    request.members.emplace_back(account.uid());
    request.memberGroupInfoSecrets.emplace_back("group_info_secret" + account.uid());
    request.memberProofs.emplace_back("proof" + account.uid());
    request.signatureInfos.emplace_back("signature" + account.uid());
    
    context.authResult = owner;
    context.requestEntity = request;

    controller.onInviteGroupMemberV3(context);
    REQUIRE(context.response.result() == http::status::ok);
}

TEST_CASE("KeysCache")
{
    bcm::RedisConfig redisCfg;
    redisCfg.ip = "127.0.0.1";
    redisCfg.port = 6379;    
    redisCfg.password = "";
    redisCfg.regkey = "";

    std::unordered_map<std::string, std::unordered_map<std::string, RedisConfig> > configs;
    configs["p0"]["0"] = redisCfg;
    configs["p0"]["1"] = redisCfg;

    RedisDbManager::Instance()->setRedisDbConfig(configs);
    bcm::GroupManagerController::KeysCache cache(10);

    AccountMock* accountMock = new AccountMock();
    std::set<std::string> uids;
    for (const auto& item : accountMock->m_accounts) {
        uids.emplace(item.first);
    }
    accountMock->setKeysUids(uids);
    std::vector<bcm::Keys> keys;
    accountMock->getKeys(uids, keys);

    REQUIRE(cache.set(13579, 34, keys) == true);
    std::vector<bcm::Keys> actual;
    REQUIRE(cache.get(13579, 34, actual) == true);

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

    REQUIRE(keys.size() == actual.size());
    for (size_t i = 0; i < keys.size(); i++) {
        const auto& k1 = keys.at(i);
        const auto& k2 = actual.at(i);
        REQUIRE(k1.deviceid() == k2.deviceid());
        REQUIRE(k1.registrationid() == k2.registrationid());
        REQUIRE(k1.identitykey() == k2.identitykey());
        signedPrekeyComp(k1.signedprekey(), k2.signedprekey());
        onetimeKeyComp(k1.onetimekey(), k2.onetimekey());
    }

    std::this_thread::sleep_for(std::chrono::seconds(11));
    REQUIRE(cache.get(13579, 34, actual) == false);
}

TEST_CASE("selectKeyDistributionCandidates")
{
    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    GroupConfig groupConfig;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    AccountMock* accountMock = new AccountMock();
    GroupsMock* groupsMock = new GroupsMock();
    GroupUsersMock* groupUsersMock = new GroupUsersMock(*groupsMock, *accountMock);
    PendingGroupUsersMock* pendingGroupUsersMock = new PendingGroupUsersMock();
    GroupMsgMock* groupMsg = new GroupMsgMock();
    GroupKeysMock* groupKeysMock = new GroupKeysMock();
    QrCodeGroupUsersMock* qrCodeGroupUsers = new QrCodeGroupUsersMock();
    GroupMsgServiceMock* groupMsgServiceMock = new GroupMsgServiceMock(bcm::RedisConfig(), dispatchManager, bcm::NoiseConfig());

    controller.m_accountsManager->m_accounts.reset(accountMock);
    controller.m_groups.reset(groupsMock);
    controller.m_groupUsers.reset(groupUsersMock);
    controller.m_pendingGroupUsers.reset(pendingGroupUsersMock);
    controller.m_groupMessage.reset(groupMsg);
    controller.m_qrCodeGroupUsers.reset(qrCodeGroupUsers);
    controller.m_groupKeys.reset(groupKeysMock);
    controller.m_groupMsgService.reset(groupMsgServiceMock);

    uint64_t gid = 10000;
    std::vector<std::string> uids;
    for (int i = 0; i < 256; i++) {
        uids.emplace_back("uid" + std::to_string(i));
    }

    groupMsgServiceMock->setLocalOnlineGroupMembers(gid, uids);
    uint total = 256;
    for (uint i = 0; i < total; i++) {
        std::set<DispatchAddress> candidates;
        uint32_t count = 5;
        controller.selectKeyDistributionCandidates(gid, i, count, candidates);
        REQUIRE(candidates.size() == count);
        std::vector<int> indexes;
        for (const auto& index : candidates) {
            int id = std::stoi(index.getUid().substr(3));
            indexes.emplace_back(id);
        }

        int sep = 0;
        for (size_t id = 1; id < indexes.size(); id++) {
            if (indexes[id - 1] + 1 != indexes[id]) {
                sep = id;
                break;
            }
        }

        for (uint32_t c = 1; c < count; c++) {
            int next = (sep + 1) % count;
            REQUIRE(indexes[next] == (indexes[sep] + 1) % total);
            sep = next;
        }
    }

}

TEST_CASE("onQueryMembersV3")
{
    initMetricsClientGroup();
    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    bcm::EncryptSenderConfig esc;
    std::shared_ptr<bcm::DispatchManager> dispatchManager = std::make_shared<bcm::DispatchManager>(bcm::DispatcherConfig(),
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   nullptr,
                                                                                                   esc);
    GroupConfig groupConfig;
    MultiDeviceConfig multiDeviceConfig;
    bcm::GroupManagerController controller(nullptr, accountsManager, dispatchManager, multiDeviceConfig, groupConfig);
    AccountMock* accountMock = new AccountMock();
    GroupsMock* groupsMock = new GroupsMock();
    GroupUsersMock* groupUsersMock = new GroupUsersMock(*groupsMock, *accountMock);
    PendingGroupUsersMock* pendingGroupUsersMock = new PendingGroupUsersMock();
    GroupMsgMock* groupMsg = new GroupMsgMock();
    controller.m_accountsManager->m_accounts.reset(accountMock);
    controller.m_groups.reset(groupsMock);
    controller.m_groupUsers.reset(groupUsersMock);
    controller.m_pendingGroupUsers.reset(pendingGroupUsersMock);
    controller.m_groupMessage.reset(groupMsg);

    QueryMembersRequestV3 req;
    bcm::Account account;
    
    req.gid = groupUsersMock->m_groupUsers.begin()->first;
    for (const auto& item : groupUsersMock->m_groupUsers.begin()->second) {
        req.uids.emplace_back(item.first);
        if (req.uids.size() == 10) {
            break;
        }
    }
    accountMock->get(req.uids.front(), account);

    HttpContext context;
    context.authResult = account;
    context.requestEntity = req;
    controller.onQueryMembersV3(context);
    REQUIRE(context.response.result() == http::status::ok);
    JsonSerializerImp<QueryMembersResponseV3> serializer;
    std::string msg;
    serializer.serialize(context.responseEntity, msg);
    std::cout << "success to query group member list.(" << msg << ")" << std::endl;

    // invalid parameters
    req.uids.clear();
    for (int i = 0; i <= 500; i++) {
        req.uids.emplace_back("uidxxx");
    }
    context.requestEntity = req;
    controller.onQueryMembersV3(context);
    REQUIRE(context.response.result() == http::status::bad_request);
}
