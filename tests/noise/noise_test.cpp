#include "../test_common.h"

#include "../../src/group/io_ctx_pool.h"
#include "../../src/group/online_msg_member_mgr.h"
#include "../../src/dispatcher/dispatch_channel.h"
#include <thread>
#include <chrono>

using namespace bcm;
using namespace bcm::dao;

static std::set<std::string> versionSupportUids = {
        "1PiMmSJHHdyUBtdJ4BWMeRb6sVJyq7Cxcu",
        "1BbCNCwXYZH6rWZJnL6N4n88UxeCccpKNV",
        "18UGLpxWEo8P54F1pRyuuQ2PtCk2zLbfSc",
        "19d2qeaxZZE5de4jervxHNTgh5zng22LZU",
        "1GqJsYrAqNa1ibsTNbb2oKYBUNrTPxfgU1",
        "1H5aMvD7GCn286cZX2Rivr9ho9au7qkzuv",
        "1H2LVQM2X8hfEKhTjePbGmYkBuLtYkncqU",
        "1HnUJWGniYKfc1CfqDRkJ3ihc4wRjYfPoB",
        "1Dow7bpvXrpoF7ZogvvSpz8PXQwXs5KPt",
        "1PUKDM4JDnAcQCrXmNKVtm6qGyQCuRnXqy"
};

static std::set<std::string> versionDisabledUids = {
        "1DPWzvDB2UyyBCpFZwTujYfmsgKkg2Wgnu",
        "1HcQUjrueUtiqTbuoBJ78pYCkNJ4ZHLCg8",
        "1H1J22TedL3o6wNTCT1cxPZaL6TiRNvwm9",
        "1DSkwoQMTXx26TVG11LVVsXWnDUtMqgerc",
        "1EPAZW4PYxx9W6kemB6TTDyDA9wQkhFaLq",
        "12RQ4TsBGCFZcrmJjjmsfuwFKf7Gs2Eq2w"
};

static std::map<uint64_t, std::set<std::string>> groupUsers = {
        {
                1, {
                           "1PiMmSJHHdyUBtdJ4BWMeRb6sVJyq7Cxcu",
                           "1BbCNCwXYZH6rWZJnL6N4n88UxeCccpKNV",
                           "18UGLpxWEo8P54F1pRyuuQ2PtCk2zLbfSc",
                           "19d2qeaxZZE5de4jervxHNTgh5zng22LZU",
                           "1GqJsYrAqNa1ibsTNbb2oKYBUNrTPxfgU1",
                           "1H5aMvD7GCn286cZX2Rivr9ho9au7qkzuv"
                   }
        },
        {
                2, {
                           "1H2LVQM2X8hfEKhTjePbGmYkBuLtYkncqU",
                           "1HnUJWGniYKfc1CfqDRkJ3ihc4wRjYfPoB",
                           "1DPWzvDB2UyyBCpFZwTujYfmsgKkg2Wgnu",
                           "1HcQUjrueUtiqTbuoBJ78pYCkNJ4ZHLCg8",
                           "1H1J22TedL3o6wNTCT1cxPZaL6TiRNvwm9",
                   }
        },
        {
                3, {
                           "1Dow7bpvXrpoF7ZogvvSpz8PXQwXs5KPt",
                           "1PUKDM4JDnAcQCrXmNKVtm6qGyQCuRnXqy",
                           "1DSkwoQMTXx26TVG11LVVsXWnDUtMqgerc",
                           "1EPAZW4PYxx9W6kemB6TTDyDA9wQkhFaLq",
                           "12RQ4TsBGCFZcrmJjjmsfuwFKf7Gs2Eq2w"
                   }
        }
};

class MockGroupUsersDao : public bcm::dao::GroupUsers
{
public:
    virtual bcm::dao::ErrorCode
    getJoinedGroupsList(const std::string& uid, std::vector<bcm::dao::UserGroupDetail>& groups) override
    {
        for (const auto& it : groupUsers) {
            if (it.second.find(uid) != it.second.end()) {
                bcm::dao::UserGroupDetail detail;
                detail.group.set_gid(it.first);
                detail.user.set_role(bcm::GroupUser_Role::GroupUser_Role_ROLE_MEMBER);
                groups.emplace_back(detail);
            }
        }

        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode
    getGroupDetailByGid(uint64_t gid, const std::string& uid, bcm::dao::UserGroupDetail& detail) override
    {
        boost::ignore_unused(gid, uid);
        detail.user.set_role(bcm::GroupUser_Role::GroupUser_Role_ROLE_MEMBER);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode insert(const bcm::GroupUser& user) override
    {
        boost::ignore_unused(user);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode insertBatch(const std::vector<bcm::GroupUser>& users) override
    {
        boost::ignore_unused(users);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode getMemberRole(uint64_t gid, const std::string& uid, bcm::GroupUser::Role& role) override
    {
        boost::ignore_unused(gid, uid, role);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode getMemberRoles(uint64_t gid, std::map<std::string, bcm::GroupUser::Role>& userRoles) override
    {
        boost::ignore_unused(gid, userRoles);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode delMember(uint64_t gid, const std::string& uid) override
    {
        boost::ignore_unused(gid, uid);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode delMemberBatch(uint64_t gid, const std::vector<std::string>& uids) override
    {
        boost::ignore_unused(gid, uids);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode getMemberBatch(uint64_t gid,
                                     const std::vector<std::string>& uids,
                                     std::vector<bcm::GroupUser>& users) override
    {
        boost::ignore_unused(gid, uids, users);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode getMemberRangeByRolesBatch(uint64_t gid, const std::vector<bcm::GroupUser::Role>& roles,
                                                 std::vector<bcm::GroupUser>& users) override
    {
        boost::ignore_unused(gid, roles, users);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode getMemberRangeByRolesBatchWithOffset(uint64_t gid,
                                                           const std::vector<bcm::GroupUser::Role>& roles,
                                                           const std::string& startUid, int count,
                                                           std::vector<bcm::GroupUser>& users) override
    {
        boost::ignore_unused(gid, roles, startUid, count, users);
        return bcm::dao::ERRORCODE_SUCCESS;
    }
    
    virtual bcm::dao::ErrorCode getJoinedGroups(const std::string& uid, std::vector<uint64_t>& gids) override
    {
        boost::ignore_unused(gids, uid);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode getGroupDetailByGidBatch(const std::vector<uint64_t>& gids, const std::string& uid,
                                               std::vector<UserGroupEntry>& entries) override
    {
        boost::ignore_unused(gids, uid, entries);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode getGroupOwner(uint64_t gid, std::string& owner) override
    {
        boost::ignore_unused(gid, owner);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode getMember(uint64_t gid, const std::string& uid, bcm::GroupUser& user) override
    {
        boost::ignore_unused(gid, uid, user);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode queryGroupMemberInfoByGid(uint64_t gid, GroupCounter& counter) override
    {
        boost::ignore_unused(gid, counter);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode queryGroupMemberInfoByGid(uint64_t gid, GroupCounter& counter, const std::string& querier,
                                                bcm::GroupUser::Role& querierRole, const std::string& nextOwner,
                                                bcm::GroupUser::Role& nextOwnerRole) override
    {
        boost::ignore_unused(gid, counter, querier, querierRole, nextOwner, nextOwnerRole);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual bcm::dao::ErrorCode update(uint64_t gid, const std::string& uid, const nlohmann::json& upData) override
    {
        boost::ignore_unused(gid, uid, upData);
        return bcm::dao::ERRORCODE_SUCCESS;
    }

    virtual ErrorCode updateIfEmpty(uint64_t gid, const std::string& uid, const nlohmann::json& upData) override
    {
        boost::ignore_unused(gid, uid, upData);
        return ERRORCODE_SUCCESS;
    }

    virtual ErrorCode getMembersOrderByCreateTime(uint64_t gid,
                                                  const std::vector<bcm::GroupUser::Role>& roles,
                                                  const std::string& startUid,
                                                  int64_t createTime,
                                                  int count,
                                                  std::vector<bcm::GroupUser>& users) override
    {
        boost::ignore_unused(gid, roles, startUid, createTime, count, users);
        return bcm::dao::ERRORCODE_SUCCESS;
    } 
    
};
/*
class MockAccountsManager : public bcm::AccountsManager
{
public:
    MockAccountsManager(uint64_t iosVer, uint64_t androidVer)
    {
        uint64_t iv = iosVer;
        uint64_t av = androidVer;
        bcm::ClientVersion_OSType type = bcm::ClientVersion_OSType_OSTYPE_ANDROID;
        for (const auto& uid : versionSupportUids) {
            bcm::Account acc;
            acc.set_uid(uid);
            bcm::Device* pd = acc.add_devices();
            pd->set_id(bcm::Device::MASTER_ID);
            bcm::ClientVersion* pcv = pd->mutable_clientversion();
            pcv->set_ostype(type);
            if (type == bcm::ClientVersion_OSType_OSTYPE_ANDROID) {
                type = bcm::ClientVersion_OSType_OSTYPE_IOS;
                pcv->set_bcmbuildcode(av);
                av++;
            } else {
                type = bcm::ClientVersion_OSType_OSTYPE_ANDROID;
                pcv->set_bcmbuildcode(iv);
                iv++;
            }
            m_mockAccounts.emplace(uid, acc);
        }

        iv = iosVer - 1;
        av = androidVer - 1;
        for (const auto& uid : versionDisabledUids) {
            bcm::Account acc;
            acc.set_uid(uid);
            bcm::Device* pd = acc.add_devices();
            pd->set_id(bcm::Device::MASTER_ID);
            bcm::ClientVersion* pcv = pd->mutable_clientversion();
            pcv->set_ostype(type);
            if (type == bcm::ClientVersion_OSType_OSTYPE_ANDROID) {
                type = bcm::ClientVersion_OSType_OSTYPE_IOS;
                pcv->set_bcmbuildcode(av);
                av--;
            } else {
                type = bcm::ClientVersion_OSType_OSTYPE_ANDROID;
                pcv->set_bcmbuildcode(iv);
                iv--;
            }
            m_mockAccounts.emplace(uid, acc);
        }
    }

    virtual bcm::dao::ErrorCode get(const std::string& uid, bcm::Account& account)
    {
        std::map<std::string, bcm::Account>::const_iterator itr = m_mockAccounts.find(uid);
        if (itr == m_mockAccounts.end()) {
            return bcm::dao::ErrorCode::ERRORCODE_NO_SUCH_DATA;
        }
        account = itr->second;
        return bcm::dao::ErrorCode::ERRORCODE_SUCCESS;
    }

    virtual bool get(const std::vector<std::string>& uids,
                     std::vector<Account>& accounts,
                     std::vector<std::string>& missedUids)
    {
        for (const auto uid : uids) {
            bcm::Account account;
            if (get(uid, account) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS) {
                accounts.emplace_back(account);
            } else {
                missedUids.push_back(uid);
            }
        }
        return !accounts.empty();
    }

private:
    std::map<std::string, bcm::Account> m_mockAccounts;
};
*/
TEST_CASE("getOnlineUsers")
{
    uint64_t iosVer = 512;
    uint64_t androidVer = 1024;
    bcm::OnlineMsgMemberMgr::GroupUsersDaoPtr pgud(new MockGroupUsersDao());
    bcm::IoCtxPool pool(5);
    bcm::OnlineMsgMemberMgr mgr(pgud,pool);
    DispatcherConfig config;
    config.concurrency = 5;
    EncryptSenderConfig esc;
    std::shared_ptr<DispatchManager> pDispatchMgr = std::make_shared<DispatchManager>(config, nullptr, nullptr, nullptr, esc);
    for (const auto& it : groupUsers) {
        for (const auto& uid : it.second) {
            mgr.handleUserOnline(DispatchAddress(uid, Device::MASTER_ID));
        }
    }
    uint64_t iv = iosVer;
    uint64_t av = androidVer;
    bcm::ClientVersion_OSType type = bcm::ClientVersion_OSType_OSTYPE_ANDROID;
    for (const auto& uid : versionSupportUids) {
        bcm::Account acc;
        acc.set_uid(uid);
        bcm::Device* pd = acc.add_devices();
        pd->set_id(bcm::Device::MASTER_ID);
        bcm::ClientVersion* pcv = pd->mutable_clientversion();
        pcv->set_ostype(type);
        acc.set_authdeviceid(pd->id());
        if (type == bcm::ClientVersion_OSType_OSTYPE_ANDROID) {
            type = bcm::ClientVersion_OSType_OSTYPE_IOS;
            pcv->set_bcmbuildcode(av);
            av++;
        } else {
            type = bcm::ClientVersion_OSType_OSTYPE_ANDROID;
            pcv->set_bcmbuildcode(iv);
            iv++;
        }

        asio::io_context ioc;
        DispatchAddress addr(uid, Device::MASTER_ID);
        std::shared_ptr<WebsocketSession> ps = std::make_shared<WebsocketSession>(nullptr, nullptr, acc);
        pDispatchMgr->replaceDispatcher(addr, std::make_shared<DispatchChannel>(ioc, addr, ps, pDispatchMgr, esc));
    }

    iv = iosVer - 1;
    av = androidVer - 1;
    for (const auto& uid : versionDisabledUids) {
        bcm::Account acc;
        acc.set_uid(uid);
        bcm::Device* pd = acc.add_devices();
        pd->set_id(bcm::Device::MASTER_ID);
        bcm::ClientVersion* pcv = pd->mutable_clientversion();
        pcv->set_ostype(type);
        acc.set_authdeviceid(pd->id());
        if (type == bcm::ClientVersion_OSType_OSTYPE_ANDROID) {
            type = bcm::ClientVersion_OSType_OSTYPE_IOS;
            pcv->set_bcmbuildcode(av);
            av--;
        } else {
            type = bcm::ClientVersion_OSType_OSTYPE_ANDROID;
            pcv->set_bcmbuildcode(iv);
            iv--;
        }

        asio::io_context ioc;
        DispatchAddress addr(uid, Device::MASTER_ID);
        std::shared_ptr<WebsocketSession> ps = std::make_shared<WebsocketSession>(nullptr, nullptr, acc);
        pDispatchMgr->replaceDispatcher(addr, std::make_shared<DispatchChannel>(ioc, addr, ps, pDispatchMgr, esc));
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    for (const auto& it : groupUsers) {
        std::set<std::string> validUids;
        for (const auto& u : versionSupportUids) {
            if (it.second.find(u) == it.second.end()) {
                validUids.insert(u);
            }
        }
        std::string lastNoiseUid("");
        std::string uid;
        for (std::set<std::string>::size_type i = 0; i <= validUids.size(); i++) {
            bcm::OnlineMsgMemberMgr::UserSet result;
            mgr.getOnlineUsers(lastNoiseUid, it.first, iosVer, androidVer, it.second.size(), result, uid, pDispatchMgr);
            uint32_t count = it.second.size() > validUids.size() ? validUids.size() : it.second.size();
            std::set<std::string>::const_iterator ivu = validUids.upper_bound(lastNoiseUid);
            REQUIRE(count == result.size());
            while (count > 0) {
                if (ivu == validUids.end()) {
                    ivu = validUids.begin();
                }
                lastNoiseUid = *ivu;
                REQUIRE(result.find(DispatchAddress(lastNoiseUid, Device::MASTER_ID)) != result.end());
                ivu++;
                count--;
            }
            REQUIRE(lastNoiseUid == uid);
        }
    }
}
