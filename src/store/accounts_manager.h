#pragma once
#include "dao/client.h"
#include "utils/log.h"
#include <memory>
#include <boost/optional.hpp>

#ifdef UNIT_TEST
#define private public
#define protected public
#endif

namespace bcm {

class ModifyAccount;

class ModifyAccountDevice {
public:
    ModifyAccountDevice(Device* targetDevice, ModifyAccount* account)
            : mTargetDevicePtr(targetDevice), mMdAccountPtr(account)
    {
    }

    void set_name(const std::string& value);
    void set_authtoken(const std::string& value);
    void set_salt(const std::string& value);
    void set_signalingkey(const std::string& value);

    void set_gcmid(const std::string& value);
    void set_umengid(const std::string& value);
    void set_apnid(const std::string& value);
    void set_voipapnid(const std::string& value);
    void set_apntype(const std::string& value);

    void set_fetchesmessages(bool value);
    void set_registrationid(uint32_t value);
    void set_version(uint32_t value);

    void mutable_signedprekey(const bcm::SignedPreKey& signedPreKey);
    void set_lastseentime(uint64_t value);
    void set_createtime(uint64_t value);

    void set_supportvoice(bool value);
    void set_supportvideo(bool value);

    void set_useragent(const std::string& value);

    void mutable_clientversion(const bcm::ClientVersion& clientVersion);

    void set_state(const ::bcm::Device_State value);
    void set_features(const std::string& value);
    void set_publicKey(const std::string& value);
    void set_accountSignature(const std::string& value);

private:
    Device* mTargetDevicePtr;
    ModifyAccount* mMdAccountPtr;
};

class ModifyContactsFilters {
public:
    ModifyContactsFilters(ContactsFilters* contactFilter, ModifyAccount* account)
            : mFilterPtr(contactFilter), mMdAccountPtr(account)
    {
    }

    void set_algo(uint32_t value);
    void set_content(const std::string& value);
    void set_version(const std::string& value);

private:
    ContactsFilters* mFilterPtr;
    ModifyAccount* mMdAccountPtr;
};

class ModifyAccount {
public:
    ModifyAccount(Account* pAcc) : mAccountPtr(pAcc)
    {
    }

    Account* getAccount()
    {
        return mAccountPtr;
    }

    const AccountField& getAccountField()
    {
        return mAccountField;
    }

    bool isModifyAccountFieldOK()
    {
        return mMissFieldName.empty();
    }

    std::string getMissFieldName()
    {
        return toString(mMissFieldName);
    }

    std::shared_ptr<ModifyAccountDevice> getMutableDevice(uint32_t devId);

    std::shared_ptr<ModifyContactsFilters> getMutableContactsFilters();

    std::shared_ptr<ModifyAccountDevice> createMutableDevice(uint32_t devId);

    //
    void set_identitykey(const std::string& value);
    void set_state(const ::bcm::Account_State value);
    void set_name(const std::string& value);
    void set_avater(const std::string& value);
    void set_nickname(const std::string& value);
    void set_ldavatar(const std::string& value);
    void set_hdavatar(const std::string& value);
    void set_profilekeys(const std::string& value);
    void set_slaveDeviceNum(uint32_t value);
    void del_device(uint32_t devId);
    void del_slave_devices();

    void mutable_privacy(const ::bcm::Account_Privacy& accPrivacy);

    // ContactsFilters
    void clear_contactsfilters();

    bool checkFieldName(const google::protobuf::Message* msg, const std::string& fieldName);
    void addStringDeviceField(Device* targetDevice, const std::string& value);
    void addContactFilterField(ContactsFilters* contactFilter, const std::string& value);
private:
    void addAccountField(const std::string& fieldName);

private:
    Account* mAccountPtr;

    AccountField  mAccountField;
    std::vector<std::string>   mMissFieldName;
};

class AccountsManager {
public:
    dao::ErrorCode get(const std::string& uid, Account& account);
    bool get(const std::vector<std::string>& uids,
                     std::vector<Account>& accounts,
                     std::vector<std::string>& missedUids);
    bool create(const Account& account);

    dao::ErrorCode getKeys(const std::set<std::string>& uids, std::vector<bcm::Keys>& keys);
    dao::ErrorCode getKeysByGid(uint64_t gid, std::vector<bcm::Keys>& keys);

    bool updateAccount(ModifyAccount& modifyAccount);
    bool updateDevice(ModifyAccount& modifyAccount, uint32_t deviceId);
    
    static boost::optional<Device&> getAuthDevice(Account& account);
    static boost::optional<const Device&> getAuthDevice(const Account& account);
    static boost::optional<Device&> getDevice(Account& account, uint32_t deviceId);
    static boost::optional<const Device&> getDevice(const Account& account, uint32_t deviceId);
    static std::string getFeatures(const Account& account, uint32_t deviceId = Device::MASTER_ID);
    static std::string getAuthDeviceName(const Account& account);
    static bool isAccountActive(const Account& account);
    static bool isDeviceActive(const Device& device);
    static bool isDevicePushable(const Device& device);
    static int64_t getAccountLastSeen(const Account& account);
    static bool isSupportVoice(const Account& account);
    static bool isSupportVideo(const Account& account);

private:
    std::shared_ptr<dao::Accounts> m_accounts{dao::ClientFactory::accounts()};
};

} // namespace bcm

#ifdef UNIT_TEST
#undef private
#undef protected
#endif
