#include "device_controller.h"
#include "account_entities.h"
#include "config/group_store_format.h"
#include "crypto/hex_encoder.h"
#include "device_entities.h"
#include "proto/dao/account.pb.h"
#include "proto/device/multi_device.pb.h"
#include "redis/hiredis_client.h"
#include "redis/redis_manager.h"
#include "websocket/websocket_session.h"
#include <auth/authenticator.h>
#include <auth/authorization_header.h>
#include <boost/algorithm/hex.hpp>
#include <crypto/random.h>
#include <fiber/fiber_pool.h>
#include <metrics_client.h>
#include <random>
#include <utils/account_helper.h>
#include <utils/log.h>
#include <utils/number_utils.h>
#include <utils/time.h>

namespace bcm {

using namespace metrics;

static constexpr char kMetricsDeviceServiceName[] = "device";
static constexpr int32_t kDeviceOnlineStatus = 1;
static constexpr int32_t kDeviceOfflineStatus = 2;
static const std::string kDeviceRequestLoginRedisPrefix = "DeviceRequest_";

DeviceController::DeviceController(std::shared_ptr<AccountsManager> accountsManager,
                                   std::shared_ptr<DispatchManager> dispatchManager,
                                   std::shared_ptr<KeysManager> keysManager,
                                   const MultiDeviceConfig& multiDeviceConfig)
    : m_accountsManager(std::move(accountsManager)), m_dispatchManager(std::move(dispatchManager)), m_keysManager(std::move(keysManager)), m_multiDeviceConfig(multiDeviceConfig)
{
}

void DeviceController::addRoutes(bcm::HttpRouter& router)
{
    router.add(http::verb::put, "/v1/devices/signin", Authenticator::AUTHTYPE_NO_AUTH,
               std::bind(&DeviceController::signin, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<AccountAttributesSigned>);

    router.add(http::verb::get, "/v1/devices", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&DeviceController::getDevices, shared_from_this(), std::placeholders::_1),
               nullptr, new JsonSerializerImp<DeviceStatusRes>);

    router.add(http::verb::post, "/v1/devices", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&DeviceController::manageDevice, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<DeviceManageAction>);

    router.add(http::verb::delete_, "/v1/devices", Authenticator::AUTHTYPE_ALLOW_SLAVE,
               std::bind(&DeviceController::logout, shared_from_this(), std::placeholders::_1));

    router.add(http::verb::post, "/v1/devices/requests", Authenticator::AUTHTYPE_NO_AUTH,
               std::bind(&DeviceController::requestLogin, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<DeviceLoginReqInfo>, new JsonSerializerImp<DeviceLoginRes>);

    router.add(http::verb::get, "/v1/devices/requests/:requestId", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&DeviceController::getLoginRequestInfo, shared_from_this(), std::placeholders::_1),
               nullptr, new JsonSerializerImp<DeviceLoginReqInfo>);

    router.add(http::verb::put, "/v1/devices/avatar/:requestId", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&DeviceController::syncAvatar, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<AvatarSyncInfo>);

    router.add(http::verb::post, "/v1/devices/authorizations", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&DeviceController::authorizeDevice, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<DeviceAuthInfo>);
}

void DeviceController::signin(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& req = context.request;
    auto& res = context.response;
    auto* attr = boost::any_cast<AccountAttributesSigned>(&context.requestEntity);

    auto agent = req["X-Signal-Agent"].to_string();
    auto auth = AuthorizationHeader::parse(req[http::field::authorization].to_string());
    auto client = AccountHelper::parseClientVersion(req["X-Client-Version"].to_string());

    auto internalError = [&](const std::string& uid, const std::string& op) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "signin", (nowInMicro() - dwStartTime), 1001);
        LOGW << "internal error, uid: " << uid << " op: " << op;
        return res.result(http::status::internal_server_error);
    };

    if (!auth) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "signin", (nowInMicro() - dwStartTime), 1002);
        LOGW << "sign up need authorization header field";
        return res.result(http::status::unauthorized);
    }

    context.statics.setUid(auth->uid());

    dao::ErrorCode ec;
    Account account;
    ec = m_accountsManager->get(auth->uid(), account);
    if (ec == dao::ERRORCODE_NO_SUCH_DATA) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "signin", (nowInMicro() - dwStartTime), 1003);
        LOGW << "account is not exist uid: " << auth->uid();
        return res.result(http::status::not_found);
    }
    if (ec != dao::ERRORCODE_SUCCESS) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "signin", (nowInMicro() - dwStartTime), 1004);
        return internalError(auth->uid(), "get account");
    }

    if (account.state() == Account::DELETED) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "signin", (nowInMicro() - dwStartTime), 1005);
        LOGI << "account is deleted, uid: " << auth->uid();
        return res.result(http::status::gone);
    }

    auto device = AccountsManager::getDevice(account, auth->deviceId());
    if (!device) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "signin", (nowInMicro() - dwStartTime), 1005);
        LOGI << "device is gone, uid: " << auth->uid() << ": " << auth->deviceId();
        return res.result(http::status::gone);
    } else if (device->state() != Device::STATE_CONFIRMED) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "signin", (nowInMicro() - dwStartTime), 1005);
        LOGI << "device is not confirmed, uid: " << auth->uid() << ": " << auth->deviceId();
        return res.result(http::status::forbidden);
    } else {
        //nothing to do
    }

    if (!AccountHelper::verifySignature(device->publickey(), auth->token(), attr->sign)) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "signin", (nowInMicro() - dwStartTime), 1006);
        LOGW << "deivce signature verify failed, uid: " << auth->uid() << ": " << auth->deviceId();
        return res.result(http::status::not_acceptable);
    }


    ModifyAccount mdAccount(&account);
    auto mDev = mdAccount.getMutableDevice(auth->deviceId());
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
    m_keysManager->clear(account.uid(), device->id());

    bool ret = m_accountsManager->updateDevice(mdAccount, device->id());
    if (!ret) {
        return internalError(auth->uid(), "udpate device");
    }

    MultiDeviceMessage multiMsg;
    multiMsg.set_type(MultiDeviceMessage::DeviceLogin);
    multiMsg.set_content(std::to_string(device->id()));
    PubSubMessage pubMessage;
    pubMessage.set_type(PubSubMessage::MULTI_DEVICE);
    pubMessage.set_content(multiMsg.SerializeAsString());
    auto address = DispatchAddress(account.uid(), Device::MASTER_ID);
    m_dispatchManager->publish(address, pubMessage.SerializeAsString());

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                         "signin", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::no_content);
}

void DeviceController::logout(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& device = *(m_accountsManager->getAuthDevice(*account));

    auto internalError = [&](const std::string& uid, const std::string& op) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "logout", (nowInMicro() - dwStartTime), 1001);
        LOGW << "internal error, uid: " << uid << ": " << device.id() << " op: " << op;
        return res.result(http::status::internal_server_error);
    };

    if (device.id() == Device::MASTER_ID) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "logout", (nowInMicro() - dwStartTime), 1006);
        LOGW << "master logout not permited" << account->uid() << ": " << device.id();
        return res.result(http::status::forbidden);
    }

    ModifyAccount mdAccount(account);
    auto devices = account->mutable_devices();
    for (auto it = devices->begin(); it != devices->end(); ++it) {
        if (it->id() == device.id()) {
            MultiDeviceMessage multiMsg;
            multiMsg.set_type(MultiDeviceMessage::DeviceLogout);
            multiMsg.set_content(std::to_string(device.id()));
            PubSubMessage pubMessage;
            pubMessage.set_type(PubSubMessage::MULTI_DEVICE);
            pubMessage.set_content(multiMsg.SerializeAsString());
            auto address = DispatchAddress(account->uid(), Device::MASTER_ID);
            m_dispatchManager->publish(address, pubMessage.SerializeAsString());
            mdAccount.del_device(device.id());
            break;
        }
    }

    bool ret = m_accountsManager->updateAccount(mdAccount);
    if (!ret) {
        return internalError(account->uid(), "udpate device");
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                         "logout", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::no_content);
}

void DeviceController::manageDevice(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto* action = boost::any_cast<DeviceManageAction>(&context.requestEntity);

    if (action->action != 1) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "manageDevice", (nowInMicro() - dwStartTime), 1005);
        return res.result(http::status::bad_request);
    }

    if (action->deviceId == Device::MASTER_ID) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "manageDevice", (nowInMicro() - dwStartTime), 1005);
        return res.result(http::status::bad_request);
    }

    auto internalError = [&](const std::string& uid, const std::string& op) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "manageDevice", (nowInMicro() - dwStartTime), 1001);
        LOGW << "internal error, uid: " << uid << " op: " << op;
        return res.result(http::status::internal_server_error);
    };

    auto devices = account->mutable_devices();
    bool hasDeviceId = false;
    auto device = devices->begin();
    for (; device != devices->end(); ++device) {
        if (device->id() == action->deviceId) {
            hasDeviceId = true;
            break;
        }
    }

    if (!hasDeviceId) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "manageDevice", (nowInMicro() - dwStartTime), 1004);
        LOGW << "not has corresponding deviceId, uid: " << account->uid() << ", deviceId: " << action->deviceId;
        return res.result(http::status::not_found);
    }

    std::string toSignContent = std::to_string(action->action) +
                                std::to_string(action->deviceId) +
                                std::to_string(action->timestamp) +
                                std::to_string(action->nounce);
    if (!AccountHelper::verifySignature(account->publickey(), toSignContent, action->signature)) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "manageDevice", (nowInMicro() - dwStartTime), 1004);
        LOGW << "signature verify failed, uid: " << account->uid();
        return res.result(http::status::not_acceptable);
    }

    ModifyAccount mdAccount(account);
    mdAccount.del_device(device->id());

    bool ret = m_accountsManager->updateAccount(mdAccount);
    if (!ret) {
        return internalError(account->uid(), "udpate device");
    }

    MultiDeviceMessage multiMsg;
    multiMsg.set_type(MultiDeviceMessage::DeviceKickedByMaster);
    multiMsg.set_content(AccountsManager::getAuthDeviceName(*account));
    PubSubMessage pubMessage;
    pubMessage.set_type(PubSubMessage::MULTI_DEVICE);
    pubMessage.set_content(multiMsg.SerializeAsString());
    auto address = DispatchAddress(account->uid(), action->deviceId);
    m_dispatchManager->publish(address, pubMessage.SerializeAsString());

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                         "manageDevice", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::no_content);
}

void DeviceController::getDevices(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);

    auto& devices = account->devices();

    DeviceStatusRes result;

    for (auto it = devices.begin(); it != devices.end(); ++it) {
        if (it->state() != Device::STATE_NORMAL) {
            continue;
        }
        DeviceStatus device;
        device.id = it->id();
        device.name = it->name();
        device.lastSeen = it->lastseentime();
        device.model = it->clientversion().phonemodel();
        PubSubMessage pubMessage;
        pubMessage.set_type(PubSubMessage::QUERY_ONLINE);
        pubMessage.set_content("OnlinePing");
        auto address = DispatchAddress(account->uid(), it->id());
        if (m_dispatchManager->publish(address, pubMessage.SerializeAsString())) {
            device.status = kDeviceOnlineStatus;
        } else {
            device.status = kDeviceOfflineStatus;
        }
        result.devices.push_back(std::move(device));
    }

    context.responseEntity = result;

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                         "manageDevice", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::ok);
}

void DeviceController::requestLogin(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* prepare = boost::any_cast<DeviceLoginReqInfo>(&context.requestEntity);

    std::string requestId = AccountHelper::publicKeyToUid(prepare->publickey());

    if (!AccountHelper::verifySignature(prepare->publickey(), std::to_string(prepare->nounce()), prepare->signature())) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "requestLogin", (nowInMicro() - dwStartTime), 1004);
        LOGW << "signature verify failed, requestId: " << requestId;
        return res.result(http::status::not_acceptable);
    }

    DeviceLoginRes result;
    result.requestId = requestId;
    result.expireTime = nowInSec() + m_multiDeviceConfig.qrcodeExpireSecs;
    prepare->set_expiretime(result.expireTime);

    std::string redisKey = kDeviceRequestLoginRedisPrefix + requestId;
    bool setRes = RedisDbManager::Instance()->set(redisKey, prepare->SerializeAsString(), m_multiDeviceConfig.qrcodeExpireSecs);
    if (!setRes) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "requestLogin", (nowInMicro() - dwStartTime), 1005);
        LOGW << "set redis error" << requestId;
        return res.result(http::status::internal_server_error);
    }

    context.responseEntity = result;
    return res.result(http::status::ok);
}

void DeviceController::getLoginRequestInfo(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& requestId = context.pathParams[":requestId"];

    std::string info;
    std::string redisKey = kDeviceRequestLoginRedisPrefix + requestId;
    bool getRes = RedisDbManager::Instance()->get(redisKey, info);

    if (!getRes || info.empty()) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "requestLogin", (nowInMicro() - dwStartTime), 1004);
        LOGW << "get requestInfo from redis error: " << requestId << ": " << account->uid();
        return res.result(http::status::not_found);
    }

    DeviceLoginReqInfo result;
    if (!result.ParseFromString(info)) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "requestLogin", (nowInMicro() - dwStartTime), 1004);
        LOGW << "parse requestInfo from redis error: " << requestId << ": " << account->uid();
        return res.result(http::status::not_found);
    }

    result.set_deviceid(account->slavedevicenum() + 2);
    context.responseEntity = result;

    return res.result(http::status::ok);
}

void DeviceController::syncAvatar(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& requestId = context.pathParams[":requestId"];
    auto* syncInfo = boost::any_cast<AvatarSyncInfo>(&context.requestEntity);

    std::string info;
    std::string redisKey = kDeviceRequestLoginRedisPrefix + requestId;
    bool getRes = RedisDbManager::Instance()->get(redisKey, info);

    if (!getRes || info.empty()) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "syncAvatar", (nowInMicro() - dwStartTime), 1004);
        LOGW << "get requestInfo from redis error: " << requestId << ": " << account->uid();
        return res.result(http::status::not_found);
    }

    DeviceLoginReqInfo result;
    if (!result.ParseFromString(info)) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "syncAvatar", (nowInMicro() - dwStartTime), 1004);
        LOGW << "parse requestInfo from redis error: " << requestId << ": " << account->uid();
        return res.result(http::status::not_found);
    }

    AvatarSyncInfo& avatarSync = *syncInfo;
    if (avatarSync.type() == 2) {
        avatarSync.set_accountpublickey(account->identitykey());
    }
    MultiDeviceMessage multiMsg;
    multiMsg.set_type(MultiDeviceMessage::DeviceAvatarSync);
    multiMsg.set_content(avatarSync.SerializeAsString());
    PubSubMessage pubMessage;
    pubMessage.set_type(PubSubMessage::MULTI_DEVICE);
    pubMessage.set_content(multiMsg.SerializeAsString());
    auto address = DispatchAddress(requestId, kDeviceRequestLoginId);
    if (!m_dispatchManager->publish(address, pubMessage.SerializeAsString())) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "syncAvatar", (nowInMicro() - dwStartTime), 1007);
        LOGW << "send syncAvatar to redis error: " << requestId << ": " << account->uid();
        return res.result(http::status::not_found);
    }

    return res.result(http::status::no_content);
}

void DeviceController::authorizeDevice(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto* authInfo = boost::any_cast<DeviceAuthInfo>(&context.requestEntity);
    auto& requestId = authInfo->requestid();

    std::string info;
    std::string redisKey = kDeviceRequestLoginRedisPrefix + requestId;
    bool getRes = RedisDbManager::Instance()->get(redisKey, info);

    if (!getRes || info.empty()) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "authorizeDevice", (nowInMicro() - dwStartTime), 1004);
        LOGW << "get requestInfo from redis error: " << requestId << ": " << account->uid();
        return res.result(http::status::not_found);
    }

    DeviceLoginReqInfo reqInfo;
    if (!reqInfo.ParseFromString(info)) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "authorizeDevice", (nowInMicro() - dwStartTime), 1004);
        LOGW << "parse requestInfo from redis error: " << requestId << ": " << account->uid();
        return res.result(http::status::not_found);
    }

    if (authInfo->deviceid() != (account->slavedevicenum() + 2)) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "authorizeDevice", (nowInMicro() - dwStartTime), 1004);
        LOGW << "device id not ok: " << authInfo->deviceid() << ": " << account->slavedevicenum();
        return res.result(http::status::bad_request);
    }

    std::string toSignContent = reqInfo.publickey() + std::to_string(authInfo->deviceid());
    if (!AccountHelper::verifySignature(account->publickey(), toSignContent, authInfo->accountsignature())) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "authorizeDevice", (nowInMicro() - dwStartTime), 1004);
        LOGW << "signature verify failed, uid: " << account->uid();
        return res.result(http::status::not_acceptable);
    }

    ModifyAccount mdAccount(account);

    auto devices = account->mutable_devices();

    for (auto it = devices->begin(); it != devices->end(); ++it) {
        if (it->id() != Device::MASTER_ID) {
            MultiDeviceMessage multiMsg;
            multiMsg.set_type(MultiDeviceMessage::DeviceKickedByOther);
            multiMsg.set_content(reqInfo.devicename());
            PubSubMessage pubMessage;
            pubMessage.set_type(PubSubMessage::MULTI_DEVICE);
            pubMessage.set_content(multiMsg.SerializeAsString());
            auto address = DispatchAddress(account->uid(), it->id());
            m_dispatchManager->publish(address, pubMessage.SerializeAsString());
        }
    }

    mdAccount.del_slave_devices();

    auto internalError = [&](const std::string& uid, const std::string& op) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "authorizeDevice", (nowInMicro() - dwStartTime), 1001);
        LOGW << "internal error, uid: " << uid << " op: " << op;
        return res.result(http::status::internal_server_error);
    };

    auto mDev = mdAccount.createMutableDevice(authInfo->deviceid());
    mDev->set_publicKey(reqInfo.publickey());
    mDev->set_accountSignature(authInfo->accountsignature());
    mDev->set_state(Device::STATE_CONFIRMED);
    mDev->set_name(reqInfo.devicename());
    mdAccount.set_slaveDeviceNum(account->slavedevicenum() + 1);

    bool ret = m_accountsManager->updateAccount(mdAccount);
    if (!ret) {
        return internalError(account->uid(), "udpate device");
    }

    DeviceAuthInfo& authMsg = *authInfo;
    authMsg.set_uid(account->uid());
    authMsg.set_accountpublickey(account->publickey());
    MultiDeviceMessage multiMsg;
    multiMsg.set_type(MultiDeviceMessage::DeviceAuth);
    multiMsg.set_content(authMsg.SerializeAsString());
    PubSubMessage pubMessage;
    pubMessage.set_type(PubSubMessage::MULTI_DEVICE);
    pubMessage.set_content(multiMsg.SerializeAsString());
    auto address = DispatchAddress(requestId, kDeviceRequestLoginId);
    if (!m_dispatchManager->publish(address, pubMessage.SerializeAsString())) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "authorizeDevice", (nowInMicro() - dwStartTime), 1007);
        LOGW << "send authorizeDevice to redis offline: " << requestId << ": " << account->uid();
        return res.result(http::status::not_found);
    }

    bool setRes = RedisDbManager::Instance()->del(redisKey);
    if (!setRes) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsDeviceServiceName,
                                                             "authorizeDevice", (nowInMicro() - dwStartTime), 1005);
        LOGW << "del redis error" << requestId;
    }

    return res.result(http::status::no_content);
}
} // namespace bcm
