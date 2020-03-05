#include "account_controller.h"
#include "account_entities.h"
#include "crypto/hex_encoder.h"
#include <random>
#include <boost/algorithm/hex.hpp>
#include <utils/account_helper.h>
#include <utils/log.h>
#include <utils/time.h>
#include <utils/number_utils.h>
#include <crypto/random.h>
#include <auth/authorization_header.h>
#include <auth/authenticator.h>
#include <fiber/fiber_pool.h>
#include <metrics_client.h>
#include "../proto/dao/account.pb.h"
#include "proto/device/multi_device.pb.h"
#include "redis/redis_manager.h"
#include "config/group_store_format.h"
#include "../store/accounts_manager.h"

namespace bcm {

using namespace metrics;

static constexpr char kMetricsAccountServiceName[] = "account";
static constexpr uint32_t kMinFeaturesLength = 8;
static constexpr uint32_t kMaxFeaturesLength = 64;
static constexpr int32_t kDeviceOnlineStatus = 1;
static constexpr int32_t kDeviceOfflineStatus = 2;

AccountsController::AccountsController(std::shared_ptr<AccountsManager> accountsManager,
                                       std::shared_ptr<dao::SignUpChallenges> challenges,
                                       std::shared_ptr<TurnTokenGenerator> turnTokenGenerator,
                                       std::shared_ptr<DispatchManager> dispatchManager,
                                       std::shared_ptr<KeysManager> keysManager,
                                       ChallengeConfig challengeConfig)
    : m_accountsManager(std::move(accountsManager))
    , m_challenges(std::move(challenges))
    , m_groupUsers(dao::ClientFactory::groupUsers())
    , m_turnTokenGenerator(std::move(turnTokenGenerator))
    , m_dispatchManager(std::move(dispatchManager))
    , m_keysManager(std::move(keysManager))
    , m_challengeConfig(std::move(challengeConfig))
{
}

void AccountsController::addRoutes(bcm::HttpRouter& router)
{
    router.add(http::verb::get, "/v1/accounts/challenge/:uid", Authenticator::AUTHTYPE_NO_AUTH,
               std::bind(&AccountsController::challenge, shared_from_this(), std::placeholders::_1),
               nullptr, new JsonSerializerImp<SignUpChallenge>);

    router.add(http::verb::put, "/v1/accounts/signup", Authenticator::AUTHTYPE_NO_AUTH,
               std::bind(&AccountsController::signup, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<AccountAttributesSigned>);

    router.add(http::verb::put, "/v1/accounts/signin", Authenticator::AUTHTYPE_NO_AUTH,
               std::bind(&AccountsController::signin, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<AccountAttributesSigned>);

    router.add(http::verb::delete_, "/v1/accounts/:uid/:signature", Authenticator::AUTHTYPE_NO_AUTH,
               std::bind(&AccountsController::destroy, shared_from_this(), std::placeholders::_1));

    router.add(http::verb::put, "/v1/accounts/features", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&AccountsController::setFeatures, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<FeaturesContent>);

    router.add(http::verb::put, "/v1/accounts/attributes", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&AccountsController::setAttributes, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<AccountAttributes>);

    router.add(http::verb::put, "/v1/accounts/apn", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&AccountsController::registerApn, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<ApnRegistration>);

    router.add(http::verb::delete_, "/v1/accounts/apn", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&AccountsController::unregisterApn, shared_from_this(), std::placeholders::_1));

    router.add(http::verb::put, "/v1/accounts/gcm", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&AccountsController::registerGcm, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<GcmRegistration>);

    router.add(http::verb::delete_, "/v1/accounts/gcm", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&AccountsController::unregisterGcm, shared_from_this(), std::placeholders::_1));

    router.add(http::verb::get, "/v1/accounts/turn", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&AccountsController::getTurnToken, shared_from_this(), std::placeholders::_1),
               nullptr, new JsonSerializerImp<TurnToken>);
}

void AccountsController::challenge(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto& uid = context.pathParams[":uid"];

    if (!AccountHelper::validUid(uid)) {
        LOGI << "uid: " << uid << " is invalid";
        res.result(http::status::bad_request);
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "challenge", (nowInMicro() - dwStartTime), 1001);
        return;
    }

    SignUpChallenge challenge;
    challenge.set_difficulty(m_challengeConfig.difficulty);
    challenge.set_nonce(SecureRandom<uint32_t>::next());
    challenge.set_timestamp(static_cast<uint64_t>(nowInMilli()));

    dao::ErrorCode ec = m_challenges->set(uid, challenge);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "uid: " << uid << " save challenge failed: " << ec;
        res.result(http::status::internal_server_error);
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "challenge", (nowInMicro() - dwStartTime), 1002);
        return;
    }

    context.responseEntity = challenge;
    res.result(http::status::ok);

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
        "challenge", (nowInMicro() - dwStartTime), 0);

}

void AccountsController::signup(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& req = context.request;
    auto& res = context.response;
    auto* attrSigned = boost::any_cast<AccountAttributesSigned>(&context.requestEntity);

    auto agent = req["X-Signal-Agent"].to_string();
    auto auth = AuthorizationHeader::parse(req[http::field::authorization].to_string());
    auto client = AccountHelper::parseClientVersion(req["X-Client-Version"].to_string());

    auto internalError = [&](const std::string& uid, const std::string& op) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "signup", (nowInMicro() - dwStartTime), 1001);
        LOGW << "internal error, uid: " << uid << " op: " << op;
        return res.result(http::status::internal_server_error);
    };

    if (attrSigned->attributes.publicKey.empty()) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "signup", (nowInMicro() - dwStartTime), 1002);
        return res.result(http::status::bad_request);
    }

    if (!auth) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "signup", (nowInMicro() - dwStartTime), 1003);
        LOGW << "sign up need authorization header field";
        return res.result(http::status::unauthorized);

    }

    context.statics.setUid(auth->uid());

    if (!AccountHelper::checkUid(auth->uid(), attrSigned->attributes.publicKey)) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "signup", (nowInMicro() - dwStartTime), 1004);
        LOGW << "uid: " << auth->uid() <<" is not match to public-key: " << attrSigned->attributes.publicKey;
        return res.result(http::status::unauthorized);
    }

    // challenge verify
    dao::ErrorCode ec;
    SignUpChallenge challenge;
    ec = m_challenges->get(auth->uid(), challenge);
    if (ec == dao::ERRORCODE_INTERNAL_ERROR) {
        return internalError(auth->uid(), "get challenge");
    }
    if (ec != dao::ERRORCODE_SUCCESS) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "signup", (nowInMicro() - dwStartTime), 1005);
        LOGW << "miss challenge for uid: " << auth->uid();
        return res.result(http::status::forbidden);
    }

    if ((nowInMilli() - challenge.timestamp()) > m_challengeConfig.expiration) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "signup", (nowInMicro() - dwStartTime), 1006);
        LOGW << "challenge is expired for uid: " << auth->uid()
             << ", spent: " << (nowInMilli() - challenge.timestamp());
        return res.result(http::status::precondition_failed);
    }

    if (!AccountHelper::verifyChallenge(auth->uid(), challenge.difficulty(), challenge.nonce(), attrSigned->nonce)) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "signup", (nowInMicro() - dwStartTime), 1007);
        LOGW << "challenge verify failed for uid: " << auth->uid();
        return res.result(http::status::precondition_failed);
    }

    LOGI << "uid: " << auth->uid() << "challenge cost: " << nowInMilli() - static_cast<int64_t>(challenge.timestamp());
    m_challenges->del(auth->uid());

    // account create
    Account account;
    ec = m_accountsManager->get(auth->uid(), account);
    if (ec == dao::ERRORCODE_INTERNAL_ERROR) {
        return internalError(auth->uid(), "get account");
    }
    if (ec != dao::ERRORCODE_NO_SUCH_DATA) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "signup", (nowInMicro() - dwStartTime), 1008);
        LOGW << "account is exist: " << auth->uid();
        return res.result(http::status::conflict);
    }

    if (!AccountHelper::verifySignature(attrSigned->attributes.publicKey, auth->token(), attrSigned->sign)) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "signup", (nowInMicro() - dwStartTime), 1009);
        LOGW << "signature verify failed for uid: " << auth->uid();
        return res.result(http::status::not_acceptable);
    }

    account.set_uid(auth->uid());
    account.set_openid(auth->uid());
    account.set_publickey(attrSigned->attributes.publicKey);
    account.set_identitykey(attrSigned->attributes.publicKey);
    account.set_name(attrSigned->attributes.name);
    account.set_nickname(attrSigned->attributes.nickname);
    account.set_state(Account::NORMAL);
    account.mutable_privacy()->set_acceptstrangermsg(true);

    Device* device = account.add_devices();
    device->set_id(static_cast<uint32_t>(auth->deviceId()));
    device->set_createtime(nowInMilli());
    device->set_name(attrSigned->attributes.deviceName);
    device->set_signalingkey(attrSigned->attributes.signalingKey);
    device->set_fetchesmessages(attrSigned->attributes.fetchesMessages);
    device->set_registrationid(static_cast<uint32_t>(attrSigned->attributes.registrationId));
    device->set_supportvoice(attrSigned->attributes.voice);
    device->set_supportvideo(attrSigned->attributes.video);
    device->set_lastseentime(todayInMilli());
    device->set_useragent(agent);
    device->set_state(Device::STATE_NORMAL);
    if (client) {
        *(device->mutable_clientversion()) = *client;
    }
    auto cert = Authenticator::getCredential(auth->token());
    device->set_authtoken(cert.token);
    device->set_salt(cert.salt);

    bool ret = m_accountsManager->create(account);
    if (!ret) {
        return internalError(auth->uid(), "create account");
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
        "signup", (nowInMicro() - dwStartTime), 0);
    MetricsClient::Instance()->counterAdd("o_regist", 1);

    return res.result(http::status::ok);
}

void AccountsController::signin(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& req = context.request;
    auto& res = context.response;
    auto* attr = boost::any_cast<AccountAttributesSigned>(&context.requestEntity);

    auto agent = req["X-Signal-Agent"].to_string();
    auto auth = AuthorizationHeader::parse(req[http::field::authorization].to_string());
    auto client = AccountHelper::parseClientVersion(req["X-Client-Version"].to_string());

    auto internalError = [&](const std::string& uid, const std::string& op) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "signin", (nowInMicro() - dwStartTime), 1001);
        LOGW << "internal error, uid: " << uid << " op: " << op;
        return res.result(http::status::internal_server_error);
    };

    if (!auth) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "signin", (nowInMicro() - dwStartTime), 1002);
        LOGW << "sign up need authorization header field";
        return res.result(http::status::unauthorized);
    }

    context.statics.setUid(auth->uid());

    dao::ErrorCode ec;
    Account account;
    ec = m_accountsManager->get(auth->uid(), account);
    if (ec == dao::ERRORCODE_NO_SUCH_DATA) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "signin", (nowInMicro() - dwStartTime), 1003);
        LOGW << "account is not exist uid: " << auth->uid();
        return res.result(http::status::not_found);
    }
    if (ec != dao::ERRORCODE_SUCCESS) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "signin", (nowInMicro() - dwStartTime), 1004);
        return internalError(auth->uid(), "get account");
    }

    if (account.state() == Account::DELETED) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "signin", (nowInMicro() - dwStartTime), 1005);
        LOGI << "account is deleted, uid: " << auth->uid();
        return res.result(http::status::gone);
    }

    if (!AccountHelper::verifySignature(account.publickey(), auth->token(), attr->sign)) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "signin", (nowInMicro() - dwStartTime), 1006);
        LOGW << "signature verify failed, uid: " << auth->uid();
        return res.result(http::status::not_acceptable);
    }
    ModifyAccount   mdAccount(&account);
    auto mDev = mdAccount.createMutableDevice(auth->deviceId());
    mDev->set_createtime(nowInMilli());
    mDev->set_name(attr->attributes.deviceName);
    mDev->set_signalingkey(attr->attributes.signalingKey);
    mDev->set_fetchesmessages(attr->attributes.fetchesMessages);
    mDev->set_registrationid(static_cast<uint32_t>(attr->attributes.registrationId));
    mDev->set_supportvoice(attr->attributes.voice);
    mDev->set_supportvideo(attr->attributes.video);
    mDev->set_lastseentime(todayInMilli());
    mDev->set_useragent(agent);
    mDev->set_state(Device::STATE_NORMAL);
    if (client) {
        mDev->mutable_clientversion(*client);
    }
    auto cert = Authenticator::getCredential(auth->token());
    mDev->set_authtoken(cert.token);
    mDev->set_salt(cert.salt);

    // clear pre-keys
    m_keysManager->clear(account.uid(), auth->deviceId());

    bool ret = m_accountsManager->updateDevice(mdAccount, auth->deviceId());
    if (!ret) {
        return internalError(auth->uid(), "udpate device");
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
        "signin", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::ok);
}

void AccountsController::destroy(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto& uid = context.pathParams[":uid"];
    auto& sign = context.pathParams[":signature"];

    auto internalError = [&](const std::string& uid, const std::string& op) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "destroy", (nowInMicro() - dwStartTime), 1001);
        LOGW << "internal error, uid: " << uid << " op: " << op;
        return res.result(http::status::internal_server_error);
    };

    dao::ErrorCode ec;
    Account account;
    ec = m_accountsManager->get(uid, account);
    if (ec == dao::ERRORCODE_NO_SUCH_DATA) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "destroy", (nowInMicro() - dwStartTime), 1002);
        LOGW << "account is not exist uid: " << uid;
        return res.result(http::status::not_found);
    }
    if (ec != dao::ERRORCODE_SUCCESS) {
        return internalError(uid, "get account");
    }

    if (account.state() == Account::DELETED) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "destroy", (nowInMicro() - dwStartTime), 1003);
        return res.result(http::status::ok);
    }

    if (!AccountHelper::verifySignature(account.publickey(), uid, sign)) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "destroy", (nowInMicro() - dwStartTime), 1004);
        LOGW << "signature verify failed, uid: " << uid;
        return res.result(http::status::not_acceptable);
    }
    
    ModifyAccount   mdAccount(&account);
    mdAccount.set_state(Account::DELETED);

    for (auto it = account.mutable_devices()->begin(); it != account.mutable_devices()->end(); ++it) {
        m_dispatchManager->kick(DispatchAddress(account.uid(), it->id()));
        std::shared_ptr<ModifyAccountDevice> mDev = mdAccount.getMutableDevice(it->id());
        mDev->set_gcmid("");
        mDev->set_umengid("");
        mDev->set_apnid("");
        mDev->set_voipapnid("");
        mDev->set_fetchesmessages(false);
        mDev->set_authtoken("");
        mDev->set_salt("");
    }

    bool ret = m_accountsManager->updateAccount(mdAccount);
    if (!ret) {
        return internalError(uid, "udpate device");
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
        "destroy", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::ok);
}

void AccountsController::setFeatures(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    context.statics.setUid(account->uid());

    auto* featuresReq = boost::any_cast<FeaturesContent>(&context.requestEntity);

    auto markMetrics = [=](int code){
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
                "setFeatures", (nowInMicro() - dwStartTime), code);
    };

    auto realContent = HexEncoder::decode(featuresReq->features);
    auto featuresLength = realContent.size();
    if (featuresLength < kMinFeaturesLength || featuresLength > kMaxFeaturesLength) {
        markMetrics(400);
        LOGE << account->uid() << ": " << "length incorrect: " << featuresLength;
        return res.result(http::status::bad_request);
    }

    auto& device = *(AccountsManager::getAuthDevice(*account));
    ModifyAccount   mdAccount(account);
    std::shared_ptr<ModifyAccountDevice> mDev = mdAccount.getMutableDevice(device.id());
    mDev->set_features(featuresReq->features);

    bool upRet = m_accountsManager->updateAccount(mdAccount);
    int ret = 0;
    if (upRet) {
        res.result(http::status::ok);
        ret = 200;
    } else {
        res.result(http::status::internal_server_error);
        ret = 500;
    }

    markMetrics(ret);
}

void AccountsController::setAttributes(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& device = *(m_accountsManager->getAuthDevice(*account));
    auto* attr = boost::any_cast<AccountAttributes>(&context.requestEntity);
    auto agent = context.request["X-Signal-Agent"].to_string();

    auto internalError = [&](const std::string& uid, const std::string& op) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "setAttributes", (nowInMicro() - dwStartTime), 1001);
        LOGW << "internal error, uid: " << uid << " op: " << op;
        return res.result(http::status::internal_server_error);
    };

    ModifyAccount   mdAccount(account);
    std::shared_ptr<ModifyAccountDevice> mDev = mdAccount.getMutableDevice(device.id());

    mDev->set_signalingkey(attr->signalingKey);
    mDev->set_registrationid(attr->registrationId);
    mDev->set_name(attr->deviceName);
    mDev->set_fetchesmessages(attr->fetchesMessages);
    mDev->set_supportvoice(attr->voice);
    mDev->set_supportvideo(attr->video);
    mDev->set_lastseentime(todayInMilli());
    mDev->set_useragent(agent);

    if (!m_accountsManager->updateAccount(mdAccount)) {
        return internalError(account->uid(), "update account");
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
        "setAttributes", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::ok);
}

void AccountsController::registerApn(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& device = *(m_accountsManager->getAuthDevice(*account));
    auto* registration = boost::any_cast<ApnRegistration>(&context.requestEntity);

    auto internalError = [&](const std::string& uid, const std::string& op) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "registerApn", (nowInMicro() - dwStartTime), 1001);
        LOGW << "internal error, uid: " << uid << " op: " << op;
        return res.result(http::status::internal_server_error);
    };
    ModifyAccount   mdAccount(account);
    std::shared_ptr<ModifyAccountDevice> mDev = mdAccount.getMutableDevice(device.id());

    mDev->set_apnid(registration->apnRegistrationId);
    mDev->set_voipapnid(registration->voipRegistrationId);
    mDev->set_apntype(registration->type);
    mDev->set_gcmid("");
    mDev->set_umengid("");
    mDev->set_fetchesmessages(true);

    if (!m_accountsManager->updateAccount(mdAccount)) {
        return internalError(account->uid(), "update account");
    }

    clearRedisDbUserPushInfo(account->uid());

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
        "registerApn", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::ok);
}

void AccountsController::unregisterApn(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& device = *(m_accountsManager->getAuthDevice(*account));

    auto internalError = [&](const std::string& uid, const std::string& op) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "unregisterApn", (nowInMicro() - dwStartTime), 1001);
        LOGW << "internal error, uid: " << uid << " op: " << op;
        return res.result(http::status::internal_server_error);
    };


    ModifyAccount   mdAccount(account);
    std::shared_ptr<ModifyAccountDevice> mDev = mdAccount.getMutableDevice(device.id());

    mDev->set_apnid("");
    mDev->set_voipapnid("");
    mDev->set_apntype("");
    mDev->set_fetchesmessages(false);
    mDev->set_state(Device::STATE_LOGOUT);

    // delete slave device
    auto devices = account->mutable_devices();
    for (auto it = devices->begin(); it != devices->end(); ++it) {
        if (it->id() != Device::MASTER_ID) {
            MultiDeviceMessage multiMsg;
            multiMsg.set_type(MultiDeviceMessage::MasterLogout);
            multiMsg.set_content(AccountsManager::getAuthDeviceName(*account));
            PubSubMessage pubMessage;
            pubMessage.set_type(PubSubMessage::MULTI_DEVICE);
            pubMessage.set_content(multiMsg.SerializeAsString());
            auto address = DispatchAddress(account->uid(), it->id());
            m_dispatchManager->publish(address, pubMessage.SerializeAsString());
        }
    }

    mdAccount.del_slave_devices();

    if (!m_accountsManager->updateAccount(mdAccount)) {
        return internalError(account->uid(), "update account");
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
        "unregisterApn", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::ok);
}

void AccountsController::registerGcm(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& device = *(m_accountsManager->getAuthDevice(*account));
    auto* registration = boost::any_cast<GcmRegistration>(&context.requestEntity);

    auto internalError = [&](const std::string& uid, const std::string& op) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "registerGcm", (nowInMicro() - dwStartTime), 1001);
        LOGW << "internal error, uid: " << uid << " op: " << op;
        return res.result(http::status::internal_server_error);
    };

    ModifyAccount   mdAccount(account);
    std::shared_ptr<ModifyAccountDevice> mDev = mdAccount.getMutableDevice(device.id());
    mDev->set_apnid("");
    mDev->set_voipapnid("");
    mDev->set_apntype("");
    mDev->set_gcmid(registration->gcmRegistrationId);
    mDev->set_umengid(registration->umengRegistrationId);
    mDev->set_fetchesmessages(registration->webSocketChannel);

    if (!m_accountsManager->updateAccount(mdAccount)) {
        return internalError(account->uid(), "update account");
    }

    clearRedisDbUserPushInfo(account->uid());

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
        "registerGcm", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::ok);
}

void AccountsController::unregisterGcm(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& device = *(m_accountsManager->getAuthDevice(*account));

    auto internalError = [&](const std::string& uid, const std::string& op) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
            "unregisterGcm", (nowInMicro() - dwStartTime), 1001);
        LOGW << "internal error, uid: " << uid << " op: " << op;
        return res.result(http::status::internal_server_error);
    };

    ModifyAccount   mdAccount(account);
    std::shared_ptr<ModifyAccountDevice> mDev = mdAccount.getMutableDevice(device.id());

    mDev->set_gcmid("");
    mDev->set_umengid("");
    mDev->set_fetchesmessages(false);
    mDev->set_state(Device::STATE_LOGOUT);

    // delete slave device
    auto devices = account->mutable_devices();
    for (auto it = devices->begin(); it != devices->end(); ++it) {
        if (it->id() != Device::MASTER_ID) {
            MultiDeviceMessage multiMsg;
            multiMsg.set_type(MultiDeviceMessage::MasterLogout);
            multiMsg.set_content(AccountsManager::getAuthDeviceName(*account));
            PubSubMessage pubMessage;
            pubMessage.set_type(PubSubMessage::MULTI_DEVICE);
            pubMessage.set_content(multiMsg.SerializeAsString());
            auto address = DispatchAddress(account->uid(), it->id());
            m_dispatchManager->publish(address, pubMessage.SerializeAsString());
        }
    }

    mdAccount.del_slave_devices();

    if (!m_accountsManager->updateAccount(mdAccount)) {
        return internalError(account->uid(), "update account");
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
        "unregisterGcm", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::ok);
}

void AccountsController::getTurnToken(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    context.responseEntity = m_turnTokenGenerator->generate();

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAccountServiceName,
        "getTurnToken", (nowInMicro() - dwStartTime), 0);

    return context.response.result(http::status::ok);
}

void AccountsController::clearRedisDbUserPushInfo(const std::string& uid)
{
    std::vector <uint64_t> gids;
    dao::ErrorCode ec = m_groupUsers->getJoinedGroups(uid, gids);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "get user joined groups error, error code: " << ec << ", uid: " << uid;
        return;
    }
    std::string hkey;
    for (auto& v : gids) {
        hkey = REDISDB_KEY_PREFIX_GROUP_USER_INFO + std::to_string(v);
        std::string val;
        bcm::GroupUserMessageIdInfo groupUserInfo;
        if (!RedisDbManager::Instance()->hget(v, hkey, uid, val)) {
            LOGE << "redisDb partition hget failed, hkey: " << hkey << ", uid: " << uid;
            continue;
        };
        if ("" != val) {
            groupUserInfo.from_string(val);
            groupUserInfo.gcmId = "";
            groupUserInfo.umengId = "";
            groupUserInfo.apnId = "";
            groupUserInfo.apnType = "";
            groupUserInfo.voipApnId = "";
            groupUserInfo.cfgFlag = GroupUserConfigPushType::NORMAL;

            if (!RedisDbManager::Instance()->hset(v, hkey, uid, groupUserInfo.to_string())) {
                LOGE << "redisDb partition hset failed, hkey: " << hkey << ", uid: " << uid << ", value: " << groupUserInfo.to_string();
            }
        }
    }
}

}
