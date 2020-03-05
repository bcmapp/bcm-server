#include <utility>

#include "authenticator.h"
#include "crypto/sha1.h"
#include "crypto/random.h"
#include "crypto/hex_encoder.h"
#include "utils/time.h"
#include <string>

namespace bcm {

Authenticator::Authenticator()
    : m_accountsManager(new AccountsManager())
{
}

Authenticator::Authenticator(std::shared_ptr<AccountsManager> accountsManager)
    : m_accountsManager(std::move(accountsManager))
{
}

bool Authenticator::verify(const Credential& credential, const std::string& userToken)
{
    auto digest = SHA1::digest(credential.salt + userToken);
    return HexEncoder::encode(digest) == credential.token;
}

Authenticator::AuthResult Authenticator::auth(const AuthorizationHeader& authHeader,
                                              const boost::optional<ClientVersion>& client,
                                              Account& account, AuthType type) const
{
    if (type == AUTHTYPE_NO_AUTH) {
        return AUTHRESULT_SUCESS;
    }

    auto deviceId = static_cast<uint32_t>(authHeader.deviceId());

    if (type == AUTHTYPE_ALLOW_MASTER) {
        if (deviceId != Device::MASTER_ID) {
            return AUTHRESULT_DEVICE_NOT_ALLOWED;
        }
    }

    if (type == AUTHTYPE_ALLOW_SLAVE) {
        if (deviceId == Device::MASTER_ID) {
            return AUTHRESULT_DEVICE_NOT_ALLOWED;
        }
    }

    auto res = m_accountsManager->get(authHeader.uid(), account);
    if (res == dao::ERRORCODE_SUCCESS) {
        if (account.state() == Account::State::Account_State_DELETED) {
            return AUTHRESULT_ACCOUNT_DELETED;
        }
        auto device = AccountsManager::getDevice(account, deviceId);
        if (!device) {
            return AUTHRESULT_DEVICE_NOT_FOUND;
        }
        if (device->state() != Device::STATE_NORMAL) {
            return AUTHRESULT_DEVICE_ABNORMAL;
        }
        account.set_authdeviceid(static_cast<uint32_t>(authHeader.deviceId()));
        if (verify(Credential{device.get().authtoken(), device.get().salt()}, authHeader.token())) {
            if (checkAndUpdateDevice(account, client)) {
                return AUTHRESULT_SUCESS;
            } else {
                return AUTHRESULT_UNKNOWN_ERROR;
            }
        } else {
            return AUTHRESULT_TOKEN_VERIFY_FAILED;
        }
    } else if (res == dao::ERRORCODE_NO_SUCH_DATA) {
        return AUTHRESULT_ACCOUNT_NOT_FOUND;
    }
    return AUTHRESULT_UNKNOWN_ERROR;
}

Authenticator::Credential Authenticator::getCredential(const std::string& userToken)
{
    Credential credential;
    auto randNum = SecureRandom<uint32_t>::next();
    credential.salt = std::to_string(randNum);
    credential.token = HexEncoder::encode(SHA1::digest(credential.salt + userToken));
    return credential;
}

static inline bool operator!=(const ClientVersion& r, const ClientVersion& l)
{
    return r.ostype() != l.ostype()
           || r.osversion() != l.osversion()
           || r.phonemodel() != l.phonemodel()
           || r.bcmversion() != l.bcmversion()
           || r.bcmbuildcode() != l.bcmbuildcode()
           || r.areacode() != l.areacode()
           || r.langcode() != l.langcode();
}

bool Authenticator::checkAndUpdateDevice(Account& account, const boost::optional<ClientVersion>& client) const
{
    bool bUpdate = false;
    auto device = AccountsManager::getAuthDevice(account);
    
    ModifyAccount   mdAccount(&account);
    std::shared_ptr<ModifyAccountDevice> mDev = mdAccount.getMutableDevice(device->id());
    
    if (static_cast<int64_t>(device->lastseentime()) != todayInMilli()) {
        bUpdate = true;
        mDev->set_lastseentime(static_cast<uint64_t>(todayInMilli()));
    }

    if (client && (*client != device->clientversion())) {
        bUpdate = true;
        mDev->mutable_clientversion(*client);
    }

    if (!bUpdate) {
        return true;
    }

    return m_accountsManager->updateDevice(mdAccount, account.authdeviceid());
}

void Authenticator::refreshAuthenticated(Account& account) const
{
    Account newOne;
    auto res = m_accountsManager->get(account.uid(), newOne);
    if (res == dao::ERRORCODE_SUCCESS) {
        newOne.set_authdeviceid(account.authdeviceid());
        account = std::move(newOne);
    } else {
        LOGW << "failed, uid: " << account.uid() << ", error" << res;
    }
}

}
