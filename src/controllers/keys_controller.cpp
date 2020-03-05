#include "keys_controller.h"
#include "keys_entities.h"
#include <utils/log.h>
#include <http/http_router.h>
#include <utils/time.h>
#include <proto/dao/account.pb.h>
#include <proto/dao/error_code.pb.h>
#include <metrics_client.h>
#include "../store/accounts_manager.h"

namespace bcm {

using namespace bcm::dao;
using namespace boost;
using namespace bcm::metrics;

static constexpr char kMetricsKeysServiceName[] = "keys";

KeysController::KeysController(std::shared_ptr<AccountsManager> accountsManager,
                               std::shared_ptr<KeysManager> keysManager,
                               const SizeCheckConfig& scCfg)
    : m_accountsManager(std::move(accountsManager))
    , m_keysManager(std::move(keysManager))
    , m_sizeCheckConfig(scCfg){

}

KeysController::~KeysController() {
}

void KeysController::addRoutes(bcm::HttpRouter& router) {
    router.add(http::verb::get, "/v2/keys", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&KeysController::getStatus, shared_from_this(), std::placeholders::_1),
               nullptr, new JsonSerializerImp<PreKeyCount>);

    router.add(http::verb::put, "/v2/keys", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&KeysController::setKeys, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<PreKeyState>);

    router.add(http::verb::get, "/v2/keys/:uid/:device_id", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&KeysController::getDeviceKeys, shared_from_this(), std::placeholders::_1),
               nullptr, new JsonSerializerImp<PreKeyResponse>);

    router.add(http::verb::put, "/v2/keys/signed", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&KeysController::setSignedKey, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<SignedPreKey>);

    router.add(http::verb::get, "/v2/keys/signed", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&KeysController::getSignedKey, shared_from_this(), std::placeholders::_1),
               nullptr, new JsonSerializerImp<SignedPreKey>);
}

void KeysController::getStatus(HttpContext& context) {
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);

    PreKeyCount keyCount{};
    auto ret = m_keysManager->getCount(account->uid(), account->authdeviceid(), keyCount.count);
    if (!ret) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeysServiceName,
            "getStatus", (nowInMicro() - dwStartTime), 1001);
        return res.result(http::status::internal_server_error);
    }

    if (keyCount.count > 0) {
        --keyCount.count;
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeysServiceName,
        "getStatus", (nowInMicro() - dwStartTime), 0);

    context.responseEntity = keyCount;
    return res.result(http::status::ok);
}

void KeysController::setKeys(HttpContext& context) {
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto* keyState = boost::any_cast<PreKeyState>(&context.requestEntity);
    auto device = AccountsManager::getAuthDevice(*account);
    bool bUpdateAccount = false;

    std::vector<OnetimeKey> keys;
    for (const auto& preKey : keyState->preKeys) {
        OnetimeKey key;
        key.set_uid(account->uid());
        key.set_deviceid(account->authdeviceid());
        key.set_keyid(preKey.keyid());
        key.set_publickey(preKey.publickey());
        keys.push_back(key);
    }

    if (keys.size() > m_sizeCheckConfig.onetimeKeySize) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeysServiceName,
                                                             "setKeys",
                                                             (nowInMicro() - dwStartTime),
                                                             1006);
        LOGE << "keys size " << keys.size()
             << " is more than the limit " << m_sizeCheckConfig.onetimeKeySize;
        res.result(http::status::bad_request);
        return;
    }

    //TODO: if it need to verify signature
    ModifyAccount   mdAccount(account);
    if (keyState->signedPreKey != device->signedprekey()) {
        std::shared_ptr<ModifyAccountDevice> mDev = mdAccount.getMutableDevice(device->id());
        if (mDev) {
            mDev->mutable_signedprekey(keyState->signedPreKey);
            bUpdateAccount = true;
        }
    }

    /*
     * forbid to modify account identityKey
    if (keyState->identityKey != account->identitykey()) {
        mdAccount.set_identitykey(keyState->identityKey);
        bUpdateAccount = true;
    }
    */

    if (bUpdateAccount) {
        if (!m_accountsManager->updateAccount(mdAccount)) {
            MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeysServiceName,
                "setKeys", (nowInMicro() - dwStartTime), 1001);
            return res.result(http::status::internal_server_error);
        }
    }


    if (!keys.empty()) {
        auto ret = m_keysManager->set(account->uid(), account->authdeviceid(),
                                      keys, account->identitykey(),
                                      device->signedprekey());
        if (!ret) {
            MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeysServiceName,
                "setKeys", (nowInMicro() - dwStartTime), 1001);
            return res.result(http::status::internal_server_error);
        }
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeysServiceName,
        "setKeys", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::ok);
}

void KeysController::getDeviceKeys(HttpContext& context) {
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    //auto* account = boost::any_cast<Account>(&context.authResult);
    auto targetUid = context.pathParams.at(":uid");
    auto targetDeviceIdSelector = context.pathParams.at(":device_id");

    Account target;
    auto ret = getAccount(targetUid, targetDeviceIdSelector, target);
    if (ret == 0) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeysServiceName,
            "getDeviceKeys", (nowInMicro() - dwStartTime), 1001);
        return res.result(http::status::not_found);
    }
    if (ret < 0) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeysServiceName,
            "getDeviceKeys", (nowInMicro() - dwStartTime), 1002);
        return res.result(http::status::internal_server_error);
    }

    uint32_t targetDeviceId{0};
    if (targetDeviceIdSelector != "*") {
        targetDeviceId = std::stoul(targetDeviceIdSelector);
    }

    std::vector<OnetimeKey> targetKeys;
    auto bRet = m_keysManager->get(targetUid,
            targetDeviceIdSelector == "*" ? boost::none : boost::optional<uint32_t>(targetDeviceId),
            targetKeys);
    if (!bRet) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeysServiceName,
                                                             "getDeviceKeys",
                                                             (nowInMicro() - dwStartTime),
                                                             1002);
        return res.result(http::status::internal_server_error);
    }

    if (targetKeys.empty()) {
        LOGD << "not pre key found for uid: " << targetUid;
    }



    PreKeyResponse keyResponse{};
    for (auto& d : *(target.mutable_devices())) {
        if (!AccountsManager::isDeviceActive(d)) {
            continue;
        }
        if (targetDeviceIdSelector != "*" && d.id() != targetDeviceId) {
            continue;
        }

        boost::optional<PreKey> preKey;
        for (auto& targetKey : targetKeys) {
            if (targetKey.deviceid() == d.id()) {
                preKey.emplace();
                preKey->set_keyid(targetKey.keyid());
                preKey->set_publickey(targetKey.publickey());
                break;
            }
        }

        if (d.has_signedprekey() || preKey) {
            PreKeyResponseItem item;
            item.deviceId = d.id();
            item.registrationId = d.registrationid();
            // to be compatible with old data
            if (d.id() == Device::MASTER_ID && d.publickey().empty()) {
                item.deviceIdentityKey = target.identitykey();
            } else {
                item.deviceIdentityKey = d.publickey();
            }
            item.accountSignature = d.accountsignature();
            if (d.has_signedprekey()) {
                item.signedPreKey = d.signedprekey();
            }
            if (preKey) {
                item.preKey.swap(preKey);
            }
            keyResponse.devices.push_back(item);
        }
    }

    if (keyResponse.devices.empty()) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeysServiceName,
            "getDeviceKeys", (nowInMicro() - dwStartTime), 1001);
        LOGW << "not pre key response for " << targetUid << ":" << targetDeviceId;
        return res.result(http::status::not_found);
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeysServiceName,
        "getDeviceKeys", (nowInMicro() - dwStartTime), 0);

    keyResponse.identityKey = target.identitykey();
    context.responseEntity = keyResponse;
    return res.result(http::status::ok);
}

int KeysController::getAccount(const std::string& uid, const std::string& deviceSelector, Account& account) {

    /* Return:
     *     < 0 ---- query db error
     *     = 0 ---- account or device is not present
     *     > 0 ---- success
     */
    auto error = m_accountsManager->get(uid, account);
    if (error == dao::ERRORCODE_NO_SUCH_DATA) {
        return 0;
    }
    if (error != dao::ERRORCODE_SUCCESS) {
        return -1;
    }
    if (!AccountsManager::isAccountActive(account)) {
        LOGW << "not account for uid: " << uid;
        return 0;
    }

    if (deviceSelector == "*") {
        return 1;
    }

    try {
        auto deviceId = std::stoul(deviceSelector);
        auto targetDevice = AccountsManager::getDevice(account, static_cast<uint32_t>(deviceId));
        if (targetDevice) {
            return 1;
        }
        LOGD << "target device " << deviceId << " is not present, uid: " << account.uid();
    } catch (std::exception& e) {
        LOGW << "device selector: " << deviceSelector << " is invalid, error: " << e.what();
    }
    return 0;
}

void KeysController::setSignedKey(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto device = AccountsManager::getAuthDevice(*account);
    auto* signedPreKey = boost::any_cast<SignedPreKey>(&context.requestEntity);

    ModifyAccount   mdAccount(account);
    std::shared_ptr<ModifyAccountDevice> mDev = mdAccount.getMutableDevice(device->id());
    mDev->mutable_signedprekey(*signedPreKey);
    if (!m_accountsManager->updateDevice(mdAccount, device->id())) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeysServiceName,
            "setSignedKey", (nowInMicro() - dwStartTime), 1001);
        return res.result(http::status::internal_server_error);
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeysServiceName,
        "setSignedKey", (nowInMicro() - dwStartTime), 0);

    //return res.result(http::status::ok);
    return res.result(http::status::no_content);
}

void KeysController::getSignedKey(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto device = AccountsManager::getAuthDevice(*account);

    if (!device->has_signedprekey()) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeysServiceName,
            "getSignedKey", (nowInMicro() - dwStartTime), 1001);
        LOGW << "not signed prekey found for: " << account->uid() << "." << device->id();
        return res.result(http::status::not_found);
    }

    context.responseEntity = device->signedprekey();

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsKeysServiceName,
        "getSignedKey", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::ok);
}

}

