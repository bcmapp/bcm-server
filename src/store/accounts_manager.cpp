#include "accounts_manager.h"
#include <crypto/base64.h>
#include <utils/time.h>
#include "../proto/dao/account.pb.h"
#include "../proto/dao/device.pb.h"
#include "../proto/dao/bloom_filters.pb.h"

namespace bcm {

dao::ErrorCode AccountsManager::get(const std::string& uid, Account& account)
{
    auto ret = m_accounts->get(uid, account);
    if (ret == dao::ERRORCODE_SUCCESS) {
        LOGT << "get account success:" << uid << ": " << account.has_contactsfilters();
        LOGT << "get account content:" << account.Utf8DebugString();
    } else {
        LOGE << "get account fail:" << uid << ":" << ret;
    }
    return ret;
}

bool AccountsManager::get(const std::vector<std::string>& uids,
                          std::vector<Account>& accounts,
                          std::vector<std::string>& missedUids)
{
    auto ret = m_accounts->get(uids, accounts, missedUids);
    if (ret != dao::ERRORCODE_SUCCESS) {
        LOGE << "get accounts fail: " << ret;
        return false;
    }

    if (missedUids.empty()) {
        LOGT << "get accounts success: " << uids.size();
    } else {
        LOGE << "get accounts request: " << uids.size() << ", response: " << accounts.size()
             << ", missed uids: " << bcm::toString(missedUids);
    }

    return true;
}

bool AccountsManager::create(const Account& account)
{
    auto ret = m_accounts->create(account);
    if (ret == dao::ERRORCODE_SUCCESS) {
        LOGT << "create account success:" << account.uid();
        return true;
    }
    LOGE << "create account fail:" << account.uid() << ":" << ret;
    return false;
}
dao::ErrorCode AccountsManager::getKeys(const std::set<std::string>& uids, std::vector<bcm::Keys>& keys)
{
    auto rc = m_accounts->getKeys(uids, keys);
    if (rc != dao::ERRORCODE_SUCCESS) {
        LOGE << "getKeys failed, uids: " << bcm::toString(uids) << ", error: " << rc;
    } else {
        LOGT << "getKeys success, uids: " << bcm::toString(uids) << ", keys: ";
        for (const auto& item : keys) {
            LOGT << item.Utf8DebugString();
        }
    }
    return rc;
}

dao::ErrorCode AccountsManager::getKeysByGid(uint64_t gid, std::vector<bcm::Keys>& keys)
{
    auto rc = m_accounts->getKeysByGid(gid, keys);
    if (rc != dao::ERRORCODE_SUCCESS) {
        LOGE << "getKeysByGid failed, gid: " << gid << ", error: " << rc;
    } else {
        LOGT << "getKeysByGid success, gid: " << gid << ", keys: ";
        for (const auto& item : keys) {
            LOGT << item.Utf8DebugString();
        }
    }
    return rc;
}

boost::optional<Device&> AccountsManager::getAuthDevice(Account& account)
{
    for (auto& d : *(account.mutable_devices())) {
        if (d.id() == account.authdeviceid()) {
            return d;
        }
    }
    return boost::none;
}

boost::optional<const Device&> AccountsManager::getAuthDevice(const Account& account)
{
    for (auto& d : account.devices()) {
        if (d.id() == account.authdeviceid()) {
            return d;
        }
    }
    return boost::none;
}

boost::optional<Device&> AccountsManager::getDevice(Account& account, uint32_t deviceId)
{
    for (auto& d : *(account.mutable_devices())) {
        if (d.id() == deviceId) {
            return d;
        }
    }
    return boost::none;
}

boost::optional<const Device&> AccountsManager::getDevice(const Account& account, uint32_t deviceId)
{
    for (const auto& d : account.devices()) {
        if (d.id() == deviceId) {
            return d;
        }
    }
    return boost::none;
}

std::string AccountsManager::getFeatures(const Account& account, uint32_t deviceId)
{
    auto device = getDevice(account, deviceId);
    if (!device) {
        return "";
    }

    return device->features();
}

std::string AccountsManager::getAuthDeviceName(const Account& account)
{
    auto device = getAuthDevice(account);
    if (!device) {
        return Base64::encode("Unknown");
    }
    if (!device->name().empty()) {
        return device->name();
    }

    if (!device->has_clientversion()) {
        return Base64::encode("Unknown");
    }
    auto clientVersion = device->clientversion();
    if (!clientVersion.phonemodel().empty()) {
        return Base64::encode(clientVersion.phonemodel());
    }
    if (clientVersion.ostype() == ClientVersion::OSTYPE_ANDROID) {
        return Base64::encode("Android");
    } else if (clientVersion.ostype() == ClientVersion::OSTYPE_IOS) {
        return Base64::encode("iOS");
    }
    return Base64::encode("Unknown");
}

bool AccountsManager::isAccountActive(const Account& account)
{
    const static int64_t kAccountExpiredTimeInMilli =  (int64_t)365 * 24 * 60 * 60 * 1000; //365days
    auto masterDevice = getDevice(account, Device::MASTER_ID);
    return (account.state() == Account::NORMAL)
           && !!masterDevice
           && isDeviceActive(*masterDevice)
           && (getAccountLastSeen(account) > (nowInMilli() - kAccountExpiredTimeInMilli));
}

const static int64_t kDeviceExpiredTimeInMilli =  (int64_t)30 * 24 * 60 * 60 * 1000; //30days

bool AccountsManager::isDeviceActive(const Device& device)
{
    bool hasChannel = device.fetchesmessages() || !device.apnid().empty()
                      || !device.umengid().empty() || !device.gcmid().empty();
    return  (device.id() == Device::MASTER_ID && hasChannel && device.has_signedprekey())
            || (device.id() != Device::MASTER_ID && hasChannel && device.has_signedprekey()
               && static_cast<int64_t>(device.lastseentime()) > (nowInMilli() - kDeviceExpiredTimeInMilli));
}

bool AccountsManager::isDevicePushable(const Device& device)
{
    bool hasPushChannel = !device.apnid().empty()
            || !device.umengid().empty() || !device.gcmid().empty();
    return hasPushChannel && static_cast<int64_t>(device.lastseentime()) > (nowInMilli() - kDeviceExpiredTimeInMilli);
}

int64_t AccountsManager::getAccountLastSeen(const Account& account)
{
    uint64_t lastSeen = 0;
    for (auto& d : account.devices()) {
        if (d.lastseentime() > lastSeen) {
            lastSeen = d.lastseentime();
        }
    }
    return static_cast<int64_t>(lastSeen);
}

bool AccountsManager::isSupportVoice(const Account& account)
{
    for (const auto& device: account.devices()) {
        if (device.supportvoice()) {
            return true;
        }
    }
    return false;
}

bool AccountsManager::isSupportVideo(const Account& account)
{
    for (const auto& device: account.devices()) {
        if (device.supportvideo()) {
            return true;
        }
    }
    return false;
}

bool AccountsManager::updateAccount(ModifyAccount& modifyAccount)
{
    if (!modifyAccount.isModifyAccountFieldOK()) {
        LOGE << "update account fail, uid: " << modifyAccount.getAccount()->uid()
             << ", missed field: " << modifyAccount.getMissFieldName();
        return false;
    }

    if (modifyAccount.getAccountField().modifyfields_size() == 0
        && (!modifyAccount.getAccountField().has_modifyfilters())
        && modifyAccount.getAccountField().devices_size() == 0) {
        LOGE << "update account is not modify, uid: " << modifyAccount.getAccount()->uid();
        return false;
    }

    auto ret = m_accounts->updateAccount(*modifyAccount.getAccount(),modifyAccount.getAccountField());
    if (ret == dao::ERRORCODE_SUCCESS) {
        LOGT << "update account success, uid: " << modifyAccount.getAccount()->uid();
        return true;
    }
    LOGE << "update account fail, uid: " << modifyAccount.getAccount()->uid() << ", res: " << ret;
    return false;
}

bool AccountsManager::updateDevice(ModifyAccount& modifyAccount, uint32_t deviceId)
{
    if (!modifyAccount.isModifyAccountFieldOK()) {
        LOGE << "update Device fail, uid: " << modifyAccount.getAccount()->uid()
             << ", missed field: " << modifyAccount.getMissFieldName();
        return false;
    }

    const AccountField& accountFields = modifyAccount.getAccountField();
    if (accountFields.devices_size() == 0) {
        LOGE << "update Device is not modify, uid: " << modifyAccount.getAccount()->uid();
        return false;
    }

    for (const auto& device: accountFields.devices()) {
        if (device.id() == deviceId) {
            auto ret = m_accounts->updateDevice(*modifyAccount.getAccount(),device);
            if (ret == dao::ERRORCODE_SUCCESS) {
                LOGT << "update Device success, uid: " << modifyAccount.getAccount()->uid();
                return true;
            }
            LOGE << "update Device fail, uid: " << modifyAccount.getAccount()->uid() << ", res: " << ret;
            return false;
        }
    }
    LOGE << "update Device not found modify device, uid: " << modifyAccount.getAccount()->uid();
    return false;
}

/////////////////
const std::string ACCOUNTS_FIELD_IDENTITYKEY = "identityKey";
const std::string ACCOUNTS_FIELD_NAME = "name";
const std::string ACCOUNTS_FIELD_AVATER = "avater";
const std::string ACCOUNTS_FIELD_STATE = "state";        // json::string  (NORMAL, DELETED)
const std::string ACCOUNTS_FIELD_PRIVACY = "privacy";    // message Privacy
const std::string ACCOUNTS_FIELD_NICKNAME = "nickname";
const std::string ACCOUNTS_FIELD_SLAVENUM = "slaveDeviceNum";
const std::string ACCOUNTS_FIELD_LDAVATAR = "ldAvatar";
const std::string ACCOUNTS_FIELD_HDAVATAR = "hdAvatar";
const std::string ACCOUNTS_FIELD_PROFILEKEYS = "profileKeys";

bool ModifyAccount::checkFieldName(const google::protobuf::Message *msg, const std::string& fieldName)
{
#ifdef UNIT_TEST
    const google::protobuf::Descriptor* descriptor = msg->GetDescriptor();
    const google::protobuf::FieldDescriptor* fieldDesc = descriptor->FindFieldByName(fieldName);
    if (fieldDesc) {
        if (fieldDesc->name() == fieldName) {
            return true;
        }
    }
    LOGE << "fail fieldName: " << fieldName;
    return false;
#else
    boost::ignore_unused(msg, fieldName);
    return true;
#endif
}

void ModifyAccount::addAccountField(const std::string& fieldName)
{
    if (!checkFieldName(mAccountPtr, fieldName)) {
        mMissFieldName.emplace_back(fieldName);
        return;
    }
    mAccountField.add_modifyfields(fieldName);
}

void ModifyAccount::addStringDeviceField(Device* targetDevice, const ::std::string& value)
{
    if (!checkFieldName(targetDevice, value)) {
        std::string errField = "dev_" + std::to_string(targetDevice->id()) + value;
        mMissFieldName.emplace_back(errField);
        return;
    }

    // update AccountField
    for (auto& d : *(mAccountField.mutable_devices())) {
        if (d.id() == targetDevice->id()) {
            d.add_modifyfields(value);
            return;
        }
    }

    // add AccountField::devices
    DeviceField *devField = mAccountField.add_devices();
    devField->set_id(targetDevice->id());
    devField->add_modifyfields(value);
    return;
}

void ModifyAccount::addContactFilterField(ContactsFilters* contactFilter, const std::string& value)
{
    if (!checkFieldName(contactFilter, value)) {
        std::string errField = "filter_" + value;
        mMissFieldName.emplace_back(errField);
        return;
    }

    ::bcm::ContactsFiltersField* mf = mAccountField.mutable_modifyfilters();
    mf->add_modifyfields(value);
}

std::shared_ptr<ModifyAccountDevice> ModifyAccount::getMutableDevice(uint32_t devId)
{
    Device* targetDevice = nullptr;
    for (auto& d : *(mAccountPtr->mutable_devices())) {
        if (d.id() == devId) {
            targetDevice = &d;
            break;
        }
    }

    if (!targetDevice) {
        return nullptr;
    }

    std::shared_ptr<ModifyAccountDevice> md = std::make_shared<ModifyAccountDevice>(targetDevice, this);
    return md;
}

std::shared_ptr<ModifyContactsFilters> ModifyAccount::getMutableContactsFilters()
{
    ContactsFilters* contactFilter = mAccountPtr->mutable_contactsfilters();
    std::shared_ptr<ModifyContactsFilters> cf = std::make_shared<ModifyContactsFilters>(contactFilter, this);
    return cf;
}

std::shared_ptr<ModifyAccountDevice> ModifyAccount::createMutableDevice(uint32_t devId)
{
    Device* targetDevice = nullptr;
    for (auto& d : *(mAccountPtr->mutable_devices())) {
        if (d.id() == devId) {
            targetDevice = &d;
            break;
        }
    }

    if (targetDevice) {
        *targetDevice = Device{};
    } else {
        targetDevice = mAccountPtr->add_devices();
    }
    targetDevice->set_id(devId);

    // update AccountField
    bool isModifyDevice = false;
    for (::bcm::DeviceField& md : *(mAccountField.mutable_devices())) {
        if (md.id() == devId) {
            md.clear_modifyfields();
            md.set_iscreate(true);
            isModifyDevice = true;
            break;
        }
    }

    if (!isModifyDevice) {
        // add AccountField::devices
        DeviceField *devField = mAccountField.add_devices();
        devField->set_id(devId);
        devField->set_iscreate(true);
    }

    std::shared_ptr<ModifyAccountDevice> md = std::make_shared<ModifyAccountDevice>(targetDevice, this);
    return md;
}

void ModifyAccount::set_identitykey(const ::std::string& value)
{
    mAccountPtr->set_identitykey(value);
    addAccountField(ACCOUNTS_FIELD_IDENTITYKEY);
}
void ModifyAccount::set_state(const ::bcm::Account_State value)
{
    mAccountPtr->set_state(value);
    addAccountField(ACCOUNTS_FIELD_STATE);
}

void ModifyAccount::clear_contactsfilters()
{
    mAccountPtr->clear_contactsfilters();
    ::bcm::ContactsFiltersField* mf = mAccountField.mutable_modifyfilters();
    mf->set_isclear(true);
    mf->clear_modifyfields();
}

void ModifyAccount::set_name(const std::string& value)
{
    mAccountPtr->set_name(value);
    addAccountField(ACCOUNTS_FIELD_NAME);
}

void ModifyAccount::set_avater(const std::string& value)
{
    mAccountPtr->set_avater(value);
    addAccountField(ACCOUNTS_FIELD_AVATER);
}

void ModifyAccount::set_nickname(const std::string& value)
{
    mAccountPtr->set_nickname(value);
    addAccountField(ACCOUNTS_FIELD_NICKNAME);
}
void ModifyAccount::set_slaveDeviceNum(uint32_t value)
{
    mAccountPtr->set_slavedevicenum(value);
    addAccountField(ACCOUNTS_FIELD_SLAVENUM);
}

void ModifyAccount::set_ldavatar(const std::string& value)
{
    mAccountPtr->set_ldavatar(value);
    addAccountField(ACCOUNTS_FIELD_LDAVATAR);
}
void ModifyAccount::set_hdavatar(const std::string& value)
{
    mAccountPtr->set_hdavatar(value);
    addAccountField(ACCOUNTS_FIELD_HDAVATAR);
}

void ModifyAccount::set_profilekeys(const std::string& value)
{
    mAccountPtr->set_profilekeys(value);
    addAccountField(ACCOUNTS_FIELD_PROFILEKEYS);
}

void ModifyAccount::del_slave_devices()
{
    auto devices = mAccountPtr->mutable_devices();
    auto it = devices->begin();
    for (; it != devices->end();) {
        if (it->id() != Device::MASTER_ID) {
            DeviceField *devField = mAccountField.add_devices();
            devField->set_id(it->id());
            devField->set_iserase(true);
            it = devices->erase(it);
        } else {
            ++it;
        }
    }
}

void ModifyAccount::del_device(uint32_t devId)
{
    auto devices = mAccountPtr->mutable_devices();
    auto it = devices->begin();
    for (; it != devices->end();) {
        if (it->id() != Device::MASTER_ID && it->id() == devId) {
            DeviceField *devField = mAccountField.add_devices();
            devField->set_id(it->id());
            devField->set_iserase(true);
            it = devices->erase(it);
            break;
        } else {
            ++it;
        }
    }
}

void ModifyAccount::mutable_privacy(const ::bcm::Account_Privacy& accPrivacy)
{
    ::bcm::Account_Privacy  *k = mAccountPtr->mutable_privacy();
    *k = accPrivacy;

    addAccountField(ACCOUNTS_FIELD_PRIVACY);
}

//////////////  ModifyAccountDevice
void ModifyAccountDevice::mutable_signedprekey(const bcm::SignedPreKey& signedPreKey)
{
    bcm::SignedPreKey  *k = mTargetDevicePtr->mutable_signedprekey();
    *k = signedPreKey;
    
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "signedPreKey");
}

void ModifyAccountDevice::mutable_clientversion(const bcm::ClientVersion& clientVersion)
{
    bcm::ClientVersion* c = mTargetDevicePtr->mutable_clientversion();
    *c = clientVersion;
    
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "clientVersion");
}

void ModifyAccountDevice::set_gcmid(const ::std::string& value)
{
    mTargetDevicePtr->set_gcmid(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "gcmId");
}

void ModifyAccountDevice::set_umengid(const ::std::string& value)
{
    mTargetDevicePtr->set_umengid(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "umengId");
}

void ModifyAccountDevice::set_apntype(const std::string& value)
{
    mTargetDevicePtr->set_apntype(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "apnType");
}

void ModifyAccountDevice::set_apnid(const ::std::string& value)
{
    mTargetDevicePtr->set_apnid(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "apnId");
}

void ModifyAccountDevice::set_voipapnid(const std::string& value)
{
    mTargetDevicePtr->set_voipapnid(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "voipApnId");
}

void ModifyAccountDevice::set_fetchesmessages(bool value)
{
    mTargetDevicePtr->set_fetchesmessages(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "fetchesMessages");
}

void ModifyAccountDevice::set_authtoken(const std::string& value)
{
    mTargetDevicePtr->set_authtoken(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "authToken");
}

void ModifyAccountDevice::set_salt(const std::string& value)
{
    mTargetDevicePtr->set_salt(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "salt");
}

void ModifyAccountDevice::set_features(const std::string& value)
{
    mTargetDevicePtr->set_features(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "features");
}

void ModifyAccountDevice::set_publicKey(const std::string& value)
{
    mTargetDevicePtr->set_publickey(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "publicKey");
}
void ModifyAccountDevice::set_accountSignature(const std::string& value)
{
    mTargetDevicePtr->set_accountsignature(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "accountSignature");

}

void ModifyAccountDevice::set_signalingkey(const std::string& value)
{
    mTargetDevicePtr->set_signalingkey(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "signalingKey");
}

void ModifyAccountDevice::set_registrationid(uint32_t value)
{
    mTargetDevicePtr->set_registrationid(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "registrationId");
}

void ModifyAccountDevice::set_version(uint32_t value)
{
    mTargetDevicePtr->set_version(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "version");
}

void ModifyAccountDevice::set_name(const std::string& value)
{
    mTargetDevicePtr->set_name(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "name");
}

void ModifyAccountDevice::set_supportvoice(bool value)
{
    mTargetDevicePtr->set_supportvoice(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "supportVoice");
}

void ModifyAccountDevice::set_supportvideo(bool value)
{
    mTargetDevicePtr->set_supportvideo(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "supportVideo");
}

void ModifyAccountDevice::set_lastseentime(uint64_t value)
{
    mTargetDevicePtr->set_lastseentime(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "lastSeenTime");
}

void ModifyAccountDevice::set_createtime(uint64_t value)
{
    mTargetDevicePtr->set_createtime(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "createTime");
}

void ModifyAccountDevice::set_useragent(const std::string& value)
{
    mTargetDevicePtr->set_useragent(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "userAgent");
}

void ModifyAccountDevice::set_state(const ::bcm::Device_State value)
{
    mTargetDevicePtr->set_state(value);
    mMdAccountPtr->addStringDeviceField(mTargetDevicePtr, "state");
}

////////////////////  ModifyContactsFilters
void ModifyContactsFilters::set_algo(uint32_t value)
{
    mFilterPtr->set_algo(value);
    mMdAccountPtr->addContactFilterField(mFilterPtr, "algo");
}

void ModifyContactsFilters::set_content(const std::string& value)
{
    mFilterPtr->set_content(value);
    mMdAccountPtr->addContactFilterField(mFilterPtr, "content");
}

void ModifyContactsFilters::set_version(const ::std::string& value)
{
    mFilterPtr->set_version(value);
    mMdAccountPtr->addContactFilterField(mFilterPtr, "version");
}


} // namespace bcm
