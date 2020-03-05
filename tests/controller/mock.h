#include "../../src/dao/accounts.h"
#include "../../tools/signal_openssl_provider.h"
#include "../../src/crypto/base64.h"
#include "../../src/utils/account_helper.h"
#include "../../src/features/bcm_features.h"
#include <cinttypes>
#include <iostream>
#include <vector>

static inline int64_t nowInMillis()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();
}

template <class T>
class IMock {
    virtual void setErrorCode(T&& ec) = 0;
};

template <class T>
class Singleton {
public:
    ~Singleton() {}

    static T& getInstance() { return instance; }

private:
    Singleton() {}
    static T instance;
};

template <class T>
T Singleton<T>::instance;

class KeyStore {
public:
    ~KeyStore(){}

    void add(const std::string& uid, const std::string& privKey)
    {
        m_privKeys[uid] = privKey;
    }

    const std::string& get(const std::string& uid)
    {
        return m_privKeys.at(uid);
    }

    KeyStore() {}

private:
    std::map<std::string, std::string> m_privKeys;
};

typedef Singleton<KeyStore> PrivateKeyStore;

class AccountMock : public bcm::dao::Accounts, public IMock<bcm::dao::ErrorCode> {
public:
    AccountMock() : m_ec(bcm::dao::ErrorCode::ERRORCODE_SUCCESS)
    {
        for (int i = 0; i < 10; i++) {
            bcm::Account account;
            generateAccount(account);
            m_accounts.emplace(account.uid(), account);
        }
    }

    ~AccountMock() {}

    virtual bcm::dao::ErrorCode create(const bcm::Account& account)
    {
        boost::ignore_unused(account);
        return bcm::dao::ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    virtual bcm::dao::ErrorCode updateAccount(const bcm::Account& account, uint32_t flags)
    {
        boost::ignore_unused(account, flags);
        return bcm::dao::ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    virtual bcm::dao::ErrorCode updateDevice(const bcm::Account& account, uint32_t deviceId)
    {
        boost::ignore_unused(account, deviceId);
        return bcm::dao::ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
    
    virtual bcm::dao::ErrorCode updateAccount(const bcm::Account& account, const bcm::AccountField& modifyField)
    {
        boost::ignore_unused(account, modifyField);
        return bcm::dao::ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
    
    virtual bcm::dao::ErrorCode updateDevice(const bcm::Account& account, const bcm::DeviceField& modifyField)
    {
        boost::ignore_unused(account, modifyField);
        return bcm::dao::ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
    
    virtual bcm::dao::ErrorCode get(const std::string& uid, bcm::Account& account)
    {
        auto it = m_accounts.find(uid);
        if (it == m_accounts.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        account = it->second;
        return m_ec;
    }

    virtual bcm::dao::ErrorCode get(const std::vector<std::string>& uids,
                                    std::vector<bcm::Account>& accounts,
                                    std::vector<std::string>& missedUids)
    {
        for (const auto& uid : uids) {
            bcm::Account account;
            if (get(uid, account) == bcm::dao::ERRORCODE_NO_SUCH_DATA) {
                missedUids.emplace_back(uid);
                continue;
            }
            accounts.emplace_back(account);
        }
        if (accounts.empty()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        return m_ec;
    }

    virtual bcm::dao::ErrorCode getKeys(const std::set<std::string>& uids, std::vector<bcm::Keys>& keys)
    {
        for (const auto& uid : uids) {
            auto user = m_accounts.find(uid);
            if (user != m_accounts.end()) {
                const bcm::Account& acc = user->second;
                for (const auto& d : acc.devices()) {
                    bcm::Keys key;
                    key.set_uid(acc.uid());
                    key.set_deviceid(d.id());
                    key.set_identitykey(acc.identitykey());
                    key.set_registrationid(d.registrationid());
                    key.set_state(d.state());
                    bcm::SignedPreKey* signedPreKey = key.mutable_signedprekey();
                    *signedPreKey = d.signedprekey();
                    auto onetimeKey = m_keys.find(acc.uid());
                    auto it = onetimeKey->second.find(d.id());
                    if (onetimeKey != m_keys.end() && it != onetimeKey->second.end()) {
                        bcm::OnetimeKey* k = key.mutable_onetimekey();
                        *k = it->second.front();
                        it->second.erase(it->second.begin());
                    }
                    keys.emplace_back(std::move(key));
                }
            }
        }
        return m_ec;
    }

    virtual bcm::dao::ErrorCode getKeysByGid(uint64_t gid, std::vector<bcm::Keys>& keys)
    {
        boost::ignore_unused(gid);
        return getKeys(m_keysUids, keys);
    }

    void setKeysUids(std::set<std::string> uids)
    {
        m_keysUids = uids;
    }

    virtual void setErrorCode(bcm::dao::ErrorCode&& ec)
    {
        m_ec = ec;
    }

    void generateAccount(bcm::Account& account)
    {
        signal_context* context = nullptr;
        signal_context_create(&context, nullptr);
        signal_context_set_crypto_provider(context, &openssl_provider);

        ec_key_pair* pair = nullptr;
        curve_generate_key_pair(context, &pair);

        signal_buffer* pubkey = nullptr;
        signal_buffer* prikey = nullptr;
        ec_public_key_serialize(&pubkey, ec_key_pair_get_public(pair));
        ec_private_key_serialize(&prikey, ec_key_pair_get_private(pair));

        const char* tmp = (const char*)signal_buffer_data(pubkey);
        std::string encodedPubkeyWithDJBType = bcm::Base64::encode(std::string(tmp, tmp + signal_buffer_len(pubkey)));
        tmp = (const char*)signal_buffer_data(prikey);
        std::string encodedPrikey = bcm::Base64::encode(std::string(tmp, tmp + signal_buffer_len(prikey)));
        std::string uid = bcm::AccountHelper::publicKeyToUid(encodedPubkeyWithDJBType, true);
        account.set_uid(uid);
        account.set_publickey(encodedPubkeyWithDJBType);
        account.set_identitykey(account.publickey());

        bcm::Device* dev = account.add_devices();
        dev->set_id(1);
        bcm::BcmFeatures features(64);
        features.addFeature(bcm::Feature::FEATURE_GROUP_V3_SUPPORT);
        dev->set_features(features.getFeatures());

        bcm::SignedPreKey* k = dev->mutable_signedprekey();
        k->set_keyid(1);
        k->set_publickey("signedprekey.publickey" + uid);
        k->set_signature("signedprekey.signature" + uid);

        for (int j = 0; j < 10; j++) {
            bcm::OnetimeKey k;
            k.set_uid(uid);
            k.set_deviceid(1);
            k.set_keyid(j);
            k.set_publickey("onetimekey.publickey" + std::to_string(j));
            m_keys[uid][1].emplace_back(std::move(k));
        }

        PrivateKeyStore::getInstance().add(account.uid(), encodedPrikey);
    }

public:
    bcm::dao::ErrorCode m_ec;
    std::map<std::string, bcm::Account> m_accounts;
    std::map<std::string, std::map<uint32_t, std::vector<bcm::OnetimeKey>>> m_keys;
    std::set<std::string> m_keysUids;
};

class GroupsMock : public bcm::dao::Groups, public IMock<bcm::dao::ErrorCode> {
public:
    GroupsMock() : m_ec(bcm::dao::ErrorCode::ERRORCODE_SUCCESS)
    {
        std::srand(std::time(nullptr));

        for (int i = 0; i < 10; i++) {
            bcm::Group group;
            generateGroup(group);
            m_groups.emplace(group.gid(), group);
        }
    }

    ~GroupsMock() {}

    virtual bcm::dao::ErrorCode create(const bcm::Group& group, uint64_t& gid)
    {
        bcm::Group g = group;
        g.set_gid((uint64_t)std::rand());
        m_groups.emplace(gid, g);
        gid = g.gid();
        return m_ec;
    }

    bcm::dao::ErrorCode createByGid(const bcm::Group& group, const uint64_t gid)
    {
        bcm::Group g = group;
        g.set_gid(gid);
        m_groups.emplace(gid, g);
        return m_ec;
    }

    virtual bcm::dao::ErrorCode getMaxMid(uint64_t groupId, uint64_t& lastMid)
    {
        boost::ignore_unused(groupId, lastMid);
        return bcm::dao::ERRORCODE_INTERNAL_ERROR;
    }

    virtual bcm::dao::ErrorCode setGroupExtensionInfo(const uint64_t gid, const std::map<std::string, std::string>& info)
    {
        boost::ignore_unused(gid, info);
        return bcm::dao::ERRORCODE_INTERNAL_ERROR;
    }
    virtual bcm::dao::ErrorCode getGroupExtensionInfo(const uint64_t gid, const std::set<std::string>& extensionKeys,
                                                      std::map<std::string, std::string>& info)
    {
        boost::ignore_unused(gid, extensionKeys, info);
        return bcm::dao::ERRORCODE_INTERNAL_ERROR;
    }

    virtual bcm::dao::ErrorCode getGidByChannel(const std::string& channel, uint64_t& gid)
    {
        boost::ignore_unused(channel, gid);
        return bcm::dao::ERRORCODE_INTERNAL_ERROR;
    }

    virtual bcm::dao::ErrorCode get(uint64_t gid, bcm::Group& group)
    {
        auto it = m_groups.find(gid);
        if (it == m_groups.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        group = it->second;
        return m_ec;
    }

    virtual bcm::dao::ErrorCode update(uint64_t gid, const nlohmann::json& upData)
    {
        const std::string GROUPS_FIELD_CREATE_TIME = "create_time";
        const std::string GROUPS_FIELD_SHARE_QR_CODE_SETTING = "share_qr_code_setting";
        const std::string GROUPS_FIELD_OWNER_CONFIRM = "owner_confirm";
        const std::string GROUPS_FIELD_SHARE_SIG = "share_sig";
        const std::string GROUPS_FIELD_SHARE_AND_OWNER_CONFIRM_SIG = "share_and_owner_confirm_sig";
        auto it = m_groups.find(gid);
        if (it == m_groups.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        for (const auto& f : upData.items()) {
            if (f.key() == "update_time") {
                it->second.set_updatetime(f.value().get<int64_t>());
                continue;
            }

            if (f.key() == "share_qr_code_setting") {
                it->second.set_shareqrcodesetting(f.value().get<std::string>());
                continue;
            }

            if (f.key() == "owner_confirm") {
                it->second.set_ownerconfirm(f.value().get<int>());
                continue;
            }

            if (f.key() == "share_sig") {
                it->second.set_sharesignature(f.value().get<std::string>());
                continue;
            }

            if (f.key() == "share_and_owner_confirm_sig") {
                it->second.set_shareandownerconfirmsignature(f.value().get<std::string>());
                continue;
            }
        }
        return m_ec;
    }

    virtual bcm::dao::ErrorCode del(uint64_t gid)
    {
        boost::ignore_unused(gid);
        return bcm::dao::ERRORCODE_INTERNAL_ERROR;
    }

    void generateGroup(bcm::Group& group)
    {
        group.set_gid((uint64_t)std::rand());
        group.set_name("group" + std::to_string(group.gid()));
        group.set_encryptstatus(bcm::Group::ENCRYPT_STATUS_ON);
        group.set_broadcast(bcm::Group::BROADCAST_OFF);
        group.set_encryptedgroupinfosecret("encrypted_group_info_secret" + std::to_string(group.gid()));
        group.set_version(static_cast<int32_t>(bcm::group::GroupVersion::GroupV3));
    }

    virtual void setErrorCode(bcm::dao::ErrorCode&& ec)
    {
        m_ec = ec;
    }

public:
    bcm::dao::ErrorCode m_ec;
    std::map<uint64_t, bcm::Group> m_groups;
};

class GroupKeysMock : public bcm::dao::GroupKeys, public IMock<bcm::dao::ErrorCode> {
public:

    virtual bcm::dao::ErrorCode insert(const bcm::GroupKeys& groupKeys)
    {
        if (nextInsertGroupKeys.creator() != "" && (nextInsertGroupKeys.version() != groupKeys.version() ||
            nextInsertGroupKeys.mode() != groupKeys.mode() ||
            nextInsertGroupKeys.groupkeys() != groupKeys.groupkeys() ||
            nextInsertGroupKeys.creator() != groupKeys.creator())) {

            return bcm::dao::ERRORCODE_INTERNAL_ERROR;
        }
        if (m_ec == bcm::dao::ERRORCODE_SUCCESS) {
            m_groupKeys.emplace_back(groupKeys);
        }

        return m_ec;
    }

    virtual bcm::dao::ErrorCode get(uint64_t gid, const std::set<int64_t>& versions, std::vector<bcm::GroupKeys>& groupKeys)
    {
        boost::ignore_unused(gid, groupKeys);
        for (auto& gk : m_groupKeys) {
            for (auto& v : versions) {
                if (v == gk.version()) {
                    groupKeys.emplace_back(gk);
                    break;
                }
            }
        }
        return m_ec;
    }

    virtual bcm::dao::ErrorCode getLatestGroupKeys(const std::set<uint64_t>& gids, std::vector<bcm::GroupKeys>& groupKeys)
    {
        for (auto& searchGid : gids) {
            bcm::GroupKeys g;
            bool found = false;
            for (auto& gk : m_groupKeys) {
                if(searchGid == gk.gid()) {
                    if (!found) {
                        g = gk;
                        found = true;
                    } else {
                        if (g.version() < gk.version()) {
                            g = gk;
                        }
                    }
                }
            }
            if (found) {
                groupKeys.emplace_back(g);
            }
        }

        return m_ec;
    }

    virtual bcm::dao::ErrorCode getLatestMode(uint64_t gid, bcm::GroupKeys::GroupKeysMode& mode)
    {
        mode = lastMode;
        boost::ignore_unused(gid);
        return m_ec;
    }

    virtual bcm::dao::ErrorCode getLatestModeBatch(const std::set<uint64_t>& gid, std::map<uint64_t /* gid */, bcm::GroupKeys::GroupKeysMode>& result)
    {
        boost::ignore_unused(gid, result);
        result = lastBatchMode;
        return m_ec;
    }

    virtual bcm::dao::ErrorCode getLatestModeAndVersion(uint64_t gid, bcm::dao::rpc::LatestModeAndVersion& mv)
    {
        boost::ignore_unused(gid, mv);
        mv.set_mode(bcm::GroupKeys::ONE_FOR_EACH);
        mv.set_version(1);
        return m_ec;
    }

    /*
     * return ERRORCODE_SUCCESS if del group keys success
     * corresponds to delGroup
     */
    virtual bcm::dao::ErrorCode clear(uint64_t gid)
    {
        boost::ignore_unused(gid);
        return m_ec;
    }

    virtual void setErrorCode(bcm::dao::ErrorCode&& ec)
    {
        m_ec = ec;
    }

    virtual void setNextInsertGroupKeys(const bcm::GroupKeys& groupKeys)
    {
        nextInsertGroupKeys = groupKeys;
    }

    virtual void setNextGetLastMode(bcm::GroupKeys::GroupKeysMode mode)
    {
        lastMode = mode;
    }

    virtual void setNextGetLastModeBatch(const std::map<uint64_t /* gid */, bcm::GroupKeys::GroupKeysMode>& result) {
        lastBatchMode = result;
    }

public:
    bcm::dao::ErrorCode m_ec;
    bcm::GroupKeys nextInsertGroupKeys;
    bcm::GroupKeys::GroupKeysMode lastMode;
    std::map<uint64_t /* gid */, bcm::GroupKeys::GroupKeysMode> lastBatchMode;
    std::vector<bcm::GroupKeys> m_groupKeys;
};

class GroupUsersMock : public bcm::dao::GroupUsers, public IMock<bcm::dao::ErrorCode> {
public:
    GroupUsersMock(GroupsMock& groups, AccountMock& accounts) : m_accounts(accounts),
                                                                m_groups(groups),
                                                                m_ec(bcm::dao::ERRORCODE_SUCCESS)
    {
        auto it = m_accounts.m_accounts.begin();
        for (const auto& group : m_groups.m_groups) {
            int64_t createTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            int i = 0;
            for (const auto& user : m_accounts.m_accounts) {
                if (it == m_accounts.m_accounts.end()) {
                    it = m_accounts.m_accounts.begin();
                }
                bcm::GroupUser gu;
                gu.set_gid(group.first);
                gu.set_uid(user.first);
                if ((i % 3) == 0) {
                    createTime--;
                }
                i++;
                gu.set_createtime(createTime);
                bcm::GroupUser::Role role = bcm::GroupUser::Role::GroupUser_Role_ROLE_MEMBER;
                if (it->first == user.first) {
                    role = bcm::GroupUser::Role::GroupUser_Role_ROLE_OWNER;
                }
                gu.set_role(role);
                m_groupUsers[gu.gid()][gu.uid()] = gu;
                ++it;
            }
        }
    }

    bcm::dao::ErrorCode getOwner(uint64_t gid, bcm::Account& owner)
    {
        auto it = m_groupUsers.find(gid);
        if (it == m_groupUsers.end()) {
            return bcm::dao::ErrorCode::ERRORCODE_NO_SUCH_DATA;
        }
        for (const auto& item : it->second) {
            if (item.second.role() == bcm::GroupUser::ROLE_OWNER) {
                return m_accounts.get(item.first, owner);
            }
        }
        return bcm::dao::ErrorCode::ERRORCODE_NO_SUCH_DATA;
    }

    virtual bcm::dao::ErrorCode getJoinedGroupsList(const std::string& uid,
                                                    std::vector<bcm::dao::UserGroupDetail>& groups)
    {
        boost::ignore_unused(uid, groups);
        return m_ec;
    }

    virtual bcm::dao::ErrorCode
    getGroupDetailByGid(uint64_t gid, const std::string& uid, bcm::dao::UserGroupDetail& detail)
    {
        boost::ignore_unused(gid, uid, detail);
        return m_ec;
    }

    virtual bcm::dao::ErrorCode insert(const bcm::GroupUser& user)
    {
        m_groupUsers[user.gid()].emplace(user.uid(), user);
        return m_ec;
    }

    virtual bcm::dao::ErrorCode insertBatch(const std::vector<bcm::GroupUser>& users)
    {
        for (const auto& u : users) {
            insert(u);
        }
        return m_ec;
    }

    virtual bcm::dao::ErrorCode getMemberRole(uint64_t gid, const std::string& uid, bcm::GroupUser::Role& role)
    {
        auto g = m_groupUsers.find(gid);
        if (g == m_groupUsers.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        auto u = g->second.find(uid);
        if (u == g->second.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        role = u->second.role();
        return m_ec;
    }

    virtual bcm::dao::ErrorCode getMemberRoles(uint64_t gid, std::map<std::string, bcm::GroupUser::Role>& userRoles)
    {
        bool empty = true;
        for (auto& item : userRoles) {
            bcm::GroupUser::Role role;
            if (getMemberRole(gid, item.first, role) == m_ec) {
                item.second = role;
                empty = false;
            }
        }
        if (empty) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        return m_ec;
    }

    virtual bcm::dao::ErrorCode delMember(uint64_t gid, const std::string& uid)
    {
        boost::ignore_unused(gid, uid);
        return m_ec;
    }

    virtual bcm::dao::ErrorCode delMemberBatch(uint64_t gid, const std::vector<std::string>& uids)
    {
        boost::ignore_unused(gid, uids);
        return m_ec;
    }

    virtual bcm::dao::ErrorCode getMemberBatch(uint64_t gid,
                                               const std::vector<std::string>& uids,
                                               std::vector<bcm::GroupUser>& users)
    {
        auto g = m_groupUsers.find(gid);
        if (g == m_groupUsers.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        for (const auto& uid : uids) {
            auto u = g->second.find(uid);
            if (u != g->second.end()) {
                users.emplace_back(u->second);
            }
        }
        
        return m_ec;
    }
    

    virtual bcm::dao::ErrorCode getMemberRangeByRolesBatch(uint64_t gid, const std::vector<bcm::GroupUser::Role>& roles,
                                                           std::vector<bcm::GroupUser>& users)
    {
        boost::ignore_unused(gid, roles, users);
        return m_ec;
    }

    virtual bcm::dao::ErrorCode getMemberRangeByRolesBatchWithOffset(uint64_t gid,
                                                                     const std::vector<bcm::GroupUser::Role>& roles,
                                                                     const std::string& startUid, int count,
                                                                     std::vector<bcm::GroupUser>& users)
    {
        boost::ignore_unused(gid, roles, startUid, count, users);
        return m_ec;
    }

    virtual bcm::dao::ErrorCode getJoinedGroups(const std::string& uid, std::vector<uint64_t>& gids)
    {
        for (auto& g : m_groupUsers) {
            for (auto& u : g.second) {
                if (u.first == uid) {
                    gids.emplace_back(g.first);
                    break;
                }
            }
        }
        return m_ec;
    }

    virtual bcm::dao::ErrorCode getGroupDetailByGidBatch(const std::vector<uint64_t>& gids,
                                                         const std::string& uid,
                                                         std::vector<bcm::dao::UserGroupEntry>& entries)
    {
        boost::ignore_unused(gids, uid, entries);
        return m_ec;
    }

    virtual bcm::dao::ErrorCode getGroupOwner(uint64_t gid, std::string& owner)
    {
        auto g = m_groupUsers.find(gid);
        if (g == m_groupUsers.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        for (const auto& item : g->second) {
            if (item.second.role() == bcm::GroupUser_Role_ROLE_OWNER) {
                owner = item.second.uid();
                return m_ec;
            }
        }
        return bcm::dao::ERRORCODE_NO_SUCH_DATA;
    }

    virtual bcm::dao::ErrorCode getMember(uint64_t gid, const std::string& uid, bcm::GroupUser& user)
    {
        auto g = m_groupUsers.find(gid);
        if (g == m_groupUsers.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        auto u = g->second.find(uid);
        if (u == g->second.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        user = u->second;
        return m_ec;
    }

    virtual bcm::dao::ErrorCode queryGroupMemberInfoByGid(uint64_t gid, bcm::dao::GroupCounter& counter)
    {
        boost::ignore_unused(gid, counter);
        counter.memberCnt = m_groupUsers[gid].size();
        return m_ec;
    }

    virtual bcm::dao::ErrorCode queryGroupMemberInfoByGid(uint64_t gid, bcm::dao::GroupCounter& counter,
                                                          const std::string& querier,
                                                          bcm::GroupUser::Role& querierRole,
                                                          const std::string& nextOwner,
                                                          bcm::GroupUser::Role& nextOwnerRole)
    {
        boost::ignore_unused(gid, counter, querier, querierRole, nextOwner, nextOwnerRole);
        counter.memberCnt = m_groupUsers[gid].size();
        querierRole = m_groupUsers[gid][querier].role();
        return m_ec;
    }

    virtual bcm::dao::ErrorCode update(uint64_t gid, const std::string& uid, const nlohmann::json& upData)
    {
        auto g = m_groupUsers.find(gid);
        if (g == m_groupUsers.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        auto u = g->second.find(uid);
        if (u == g->second.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        bcm::GroupUser& user = u->second;
        for (const auto& f : upData.items()) {
            if (f.key() == "group_info_secret") {
                user.set_groupinfosecret(f.value().get<std::string>());
                continue;
            }

            if (f.key() == "encrypted_key") {
                user.set_encryptedkey(f.value().get<std::string>());
                continue;
            }

            if (f.key() == "update_time") {
                user.set_updatetime(f.value().get<int64_t>());
                continue;
            }
        }
        return m_ec;
    }

    virtual bcm::dao::ErrorCode updateIfEmpty(uint64_t gid, const std::string& uid, const nlohmann::json& upData) override
    {
        auto g = m_groupUsers.find(gid);
        if (g == m_groupUsers.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        auto u = g->second.find(uid);
        if (u == g->second.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        bcm::GroupUser user = u->second;
        for (const auto& f : upData.items()) {
            if (f.key() == "group_info_secret") {
                if (!user.groupinfosecret().empty()) {
                    return bcm::dao::ERRORCODE_ALREADY_EXSITED;
                }
                user.set_groupinfosecret(f.value().get<std::string>());
                continue;
            }

            if (f.key() == "encrypted_key") {
                if (!user.encryptedkey().empty()) {
                    return bcm::dao::ERRORCODE_ALREADY_EXSITED;
                }
                user.set_encryptedkey(f.value().get<std::string>());
                continue;
            }

            if (f.key() == "update_time") {
                user.set_updatetime(f.value().get<int64_t>());
                continue;
            }
        }
        u->second = user;
        return m_ec;
    }

    virtual bcm::dao::ErrorCode del(uint64_t gid)
    {
        boost::ignore_unused(gid);
        return bcm::dao::ERRORCODE_INTERNAL_ERROR;
    }

    virtual bcm::dao::ErrorCode getMembersOrderByCreateTime(uint64_t gid,
                                                            const std::vector<bcm::GroupUser::Role>& roles,
                                                            const std::string& startUid,
                                                            int64_t createTime,
                                                            int count,
                                                            std::vector<bcm::GroupUser>& users)
    {
        boost::ignore_unused(roles);
        auto it = m_groupUsers.find(gid);
        if (it == m_groupUsers.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        std::map<std::string, bcm::GroupUser> ordered;
        for (const auto& item : it->second) {
            std::string k;
            composeOrderKey(item.second.createtime(), item.second.uid(), k);
            ordered[k] = item.second;
        }
        std::string pos;
        composeOrderKey(createTime, startUid, pos);
        auto start = ordered.upper_bound(pos);
        if (start == ordered.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        int limit = count;
        for (; start != ordered.end(); start++) {
            if (count != 0 && limit == 0) {
                break;
            }
            users.emplace_back(start->second);
            limit--;
        }
        return m_ec;
    }

    void composeOrderKey(int64_t createTime, const std::string& uid, std::string& key)
    {
        char buf[128] = {0};
        snprintf(buf, 128, "%020" PRId64 "%s", createTime, uid.c_str());
        key.assign(buf, buf + std::strlen(buf));
    }

    void getOrderedUsers(uint64_t gid, std::map<std::string, bcm::GroupUser>& ordered)
    {
        auto it = m_groupUsers.find(gid);
        if (it == m_groupUsers.end()) {
            ordered.clear();
            return;
        }
        for (const auto& item : it->second) {
            std::string k;
            composeOrderKey(item.second.createtime(), item.second.uid(), k);
            ordered[k] = item.second;
        }
    }

    virtual void setErrorCode(bcm::dao::ErrorCode&& ec)
    {
        m_ec = ec;
    }

public:
    AccountMock& m_accounts;
    GroupsMock& m_groups;
    bcm::dao::ErrorCode m_ec;
    std::map<uint64_t, std::map<std::string, bcm::GroupUser>> m_groupUsers;
};

class PendingGroupUsersMock : public bcm::dao::PendingGroupUsers, public IMock<bcm::dao::ErrorCode> {
public:
    PendingGroupUsersMock() : m_ec(bcm::dao::ERRORCODE_SUCCESS)
    {

    }

    virtual bcm::dao::ErrorCode set(const bcm::PendingGroupUser& user)
    {
        m_pendingGroupUsers[user.gid()][user.uid()] = user;
        return m_ec;
    }

    virtual bcm::dao::ErrorCode query(uint64_t gid,
                            const std::string& startUid,
                            int count,
                            std::vector<bcm::PendingGroupUser>& result)
    {
        auto it = m_pendingGroupUsers.find(gid);
        if (it == m_pendingGroupUsers.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        auto u = it->second.upper_bound(startUid);
        if (u == it->second.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }
        if (count == 0) {
            return m_ec;
        }
        do {
            result.emplace_back(u->second);
            ++u;
            count--;
        } while (u != it->second.end() || count == 0);
        return m_ec;
    }

    virtual bcm::dao::ErrorCode del(uint64_t gid, std::set<std::string> uids)
    {
        auto it = m_pendingGroupUsers.find(gid);
        if (it == m_pendingGroupUsers.end()) {
            return m_ec;
        }
        for (const auto uid : uids) {
            it->second.erase(uid);
        }
        return m_ec;
    }

    virtual bcm::dao::ErrorCode clear(uint64_t gid)
    {
        auto it = m_pendingGroupUsers.find(gid);
        if (it == m_pendingGroupUsers.end()) {
            return m_ec;
        }
        it->second.clear();
        return m_ec;
    }

    virtual void setErrorCode(bcm::dao::ErrorCode&& ec)
    {
        m_ec = ec;
    }

public:
    bcm::dao::ErrorCode m_ec;
    std::map<uint64_t, std::map<std::string, bcm::PendingGroupUser>> m_pendingGroupUsers;
};

class GroupMsgMock : public bcm::dao::GroupMsgs, public IMock<bcm::dao::ErrorCode> {
public:
    GroupMsgMock() : m_ec(bcm::dao::ERRORCODE_SUCCESS) {
        std::srand(std::time(nullptr));
        m_initMid = std::rand();
    }

    ~GroupMsgMock() {}

    virtual bcm::dao::ErrorCode insert(const bcm::GroupMsg& msg, uint64_t& mid)
    {
        m_initMid++;
        mid = m_initMid;
        m_groupMsgs[msg.gid()][mid] = msg;
        return m_ec;
    }

    virtual bcm::dao::ErrorCode get(uint64_t groupId, uint64_t mid, bcm::GroupMsg& msg)
    {
        auto it = m_groupMsgs.find(groupId);
        if (it == m_groupMsgs.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }

        auto m = it->second.find(mid);
        if (m == it->second.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }

        msg = m->second;
        return m_ec;
    }

    virtual bcm::dao::ErrorCode batchGet(uint64_t groupId,
                                         uint64_t from,
                                         uint64_t to,
                                         uint64_t limit,
                                         bcm::GroupUser::Role role,
                                         bool supportRrecall,
                                         std::vector<bcm::GroupMsg>& msgs)
    {
        boost::ignore_unused(role, supportRrecall);
        if (limit == 0) {
            return m_ec;
        }
        auto it = m_groupMsgs.find(groupId);
        if (it == m_groupMsgs.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }

        auto start = it->second.upper_bound(from);
        auto end = it->second.end();
        if (to != 0) {
            end = it->second.lower_bound(to);
        }
        while (start != end && limit > 0) {
            msgs.emplace_back(start->second);
            limit--;
        }

        return m_ec;
    }

    virtual bcm::dao::ErrorCode recall(const std::string& sourceExtra,
                                       const std::string& uid,
                                       uint64_t gid,
                                       uint64_t mid,
                                       uint64_t& newMid)
    {
        boost::ignore_unused(sourceExtra, uid, gid, mid, newMid);
        return bcm::dao::ERRORCODE_INTERNAL_ERROR;
    }

    virtual void setErrorCode(bcm::dao::ErrorCode&& ec)
    {
        m_ec = ec;
    }

public:
    bcm::dao::ErrorCode m_ec;
    std::map<uint64_t, std::map<uint64_t, bcm::GroupMsg>> m_groupMsgs;
    uint64_t m_initMid;
};

class QrCodeGroupUsersMock : public bcm::dao::QrCodeGroupUsers, public IMock<bcm::dao::ErrorCode> {
public:
    QrCodeGroupUsersMock() : m_ec(bcm::dao::ERRORCODE_SUCCESS) {std::srand(std::time(nullptr));}

    ~QrCodeGroupUsersMock() {}

    virtual bcm::dao::ErrorCode set(const bcm::QrCodeGroupUser& user, int64_t ttl)
    {
        boost::ignore_unused(ttl);
        m_qrCodeGroupUsers[user.gid()][user.uid()] = user;
        return m_ec;
    }

    virtual bcm::dao::ErrorCode get(uint64_t gid, const std::string& uid, bcm::QrCodeGroupUser& user)
    {
        auto it = m_qrCodeGroupUsers.find(gid);
        if (it == m_qrCodeGroupUsers.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }

        auto m = it->second.find(uid);
        if (m == it->second.end()) {
            return bcm::dao::ERRORCODE_NO_SUCH_DATA;
        }

        user = m->second;
        return m_ec;
    }

    virtual void setErrorCode(bcm::dao::ErrorCode&& ec)
    {
        m_ec = ec;
    }

public:
    bcm::dao::ErrorCode m_ec;
    std::map<uint64_t, std::map<std::string, bcm::QrCodeGroupUser>> m_qrCodeGroupUsers;
};

class GroupMsgServiceMock : public bcm::GroupMsgService, public IMock<bcm::dao::ErrorCode> {
public:
    GroupMsgServiceMock(const bcm::RedisConfig redisCfg, 
                        std::shared_ptr<bcm::DispatchManager> dispatchMgr,
                        const bcm::NoiseConfig& noiseCfg)
        : GroupMsgService(redisCfg, dispatchMgr, noiseCfg),
          m_ec(bcm::dao::ErrorCode::ERRORCODE_SUCCESS),
          m_onlineGroupMembers()
    {

    }
    
    virtual void getLocalOnlineGroupMembers(uint64_t gid, int count, std::vector<std::string>& uids)
    {
        boost::ignore_unused(count);
        auto it = m_onlineGroupMembers.find(gid);
        if (it == m_onlineGroupMembers.end()) {
            return;
        }
        uids = it->second;
    }

    void setLocalOnlineGroupMembers(uint64_t gid, const std::vector<std::string>& uids)
    {
        m_onlineGroupMembers[gid] = uids;
    }

    virtual void setErrorCode(bcm::dao::ErrorCode&& ec)
    {
        m_ec = ec;
    }

public:
    bcm::dao::ErrorCode m_ec;
    std::map<uint64_t, std::vector<std::string>> m_onlineGroupMembers;
};
