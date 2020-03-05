#include "profile_controller.h"
#include "profile_entities.h"
#include "contact_entities.h"
#include "utils/log.h"
#include "auth/authorization_header.h"
#include "auth/authenticator.h"
#include "utils/account_helper.h"
#include "fiber/fiber_pool.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/algorithm/string.hpp>
#include "crypto/base64.h"
#include <metrics_client.h>
#include "utils/time.h"
#include "../proto/dao/account.pb.h"
#include "../store/accounts_manager.h"

namespace bcm {

using namespace metrics;

const std::string ProfileController::kAvatarPrefix = "avatar/";
const std::string ProfileController::kDeletedAccountName = "RGVsZXRlZCBBY2NvdW50"; // "Deleted Account"
static constexpr char kMetricsProfileServiceName[] = "profile";
static Account::Privacy kDefaultPrivacy;

ProfileController::ProfileController(
        std::shared_ptr<AccountsManager> accountsManager
        , std::shared_ptr<Authenticator> authenticator)
    : m_accountsManager(accountsManager)
    , m_authenticator(authenticator)
{
    kDefaultPrivacy.set_acceptstrangermsg(true);
}

void ProfileController::addRoutes(HttpRouter& router)
{
    router.add(http::verb::get, "/v1/profile/:uid", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&ProfileController::getSingleProfile, shared_from_this(), std::placeholders::_1),
               nullptr, new JsonSerializerImp<Profile>);

    router.add(http::verb::put, "/v1/profile", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&ProfileController::getProfileBatch, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<ContactTokens>, new JsonSerializerImp<ProfileMap>);

    router.add(http::verb::put, "/v1/profile/privacy", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&ProfileController::setPrivacy, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<Account::Privacy>);

    router.add(http::verb::put, "/v1/profile/namePlaintext/:name", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&ProfileController::setName, shared_from_this(), std::placeholders::_1));

    router.add(http::verb::get, "/v1/profile/version/:version", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&ProfileController::setVersion, shared_from_this(), std::placeholders::_1));

    router.add(http::verb::put, "/v1/profile/nickname/:nickname", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&ProfileController::setNickname, shared_from_this(), std::placeholders::_1));

    router.add(http::verb::put, "/v1/profile/avatar", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&ProfileController::setAvatar2, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<AvatarBundle>);

    router.add(http::verb::put, "/v1/profile/keys", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&ProfileController::setKeys, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<JsonInside>);

    router.add(http::verb::get, "/v1/profile/keys", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&ProfileController::getKeys, shared_from_this(), std::placeholders::_1),
               nullptr, new JsonSerializerImp<JsonInside>);

}

void ProfileController::getSingleProfile(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();

    auto& res = context.response;
    auto& account = boost::any_cast<Account&>(context.authResult);
    auto& targetUid = context.pathParams[":uid"];

    if (targetUid.empty()) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsProfileServiceName,
            "getSingleProfile", (nowInMicro() - dwStartTime), 1001);
        LOGW << "uid empty";
        return res.result(http::status::bad_request);
    }

    if (account.uid() == targetUid) {
        Profile profile;
        profile.identityKey = account.identitykey();
        profile.nickname = account.nickname();
        profile.ldAvatar = account.ldavatar();
        profile.hdAvatar = account.hdavatar();
        profile.supportVoice = AccountsManager::isSupportVoice(account);
        profile.supportVideo = AccountsManager::isSupportVideo(account);
        profile.privacy = account.has_privacy() ? account.privacy() : kDefaultPrivacy;
        profile.state = account.state();
        profile.features = AccountsManager::getFeatures(account);

        if (!account.encryptnumber().empty()) {
            profile.encryptNumber = account.encryptnumber();
            profile.numberPubkey = account.numberpubkey();
        } else {
            // TODO deprecated
            profile.phoneNum = account.phonenumber();
        }

        // TODO deprecated
        profile.name = account.name();
        profile.avatar = account.avater();

        context.responseEntity = profile;
        return res.result(http::status::ok);
    }

    auto ec = m_accountsManager->get(targetUid, account);
    if (ec == dao::ERRORCODE_NO_SUCH_DATA) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsProfileServiceName,
                "getSingleProfile", (nowInMicro() - dwStartTime), 1002);
        return res.result(http::status::not_found);
    }
    if (ec != dao::ERRORCODE_SUCCESS) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsProfileServiceName,
                "getSingleProfile", (nowInMicro() - dwStartTime), 1003);
        return res.result(http::status::internal_server_error);
    }

    LOGT << "get account info:" << account.Utf8DebugString();
    if (account.state() == Account::DELETED) {
        Profile profile;
        profile.state = Account::DELETED;
        context.responseEntity = profile;
        return res.result(http::status::ok);
    }

    Profile profile;
    profile.identityKey = account.identitykey();
    profile.nickname = account.nickname();
    profile.ldAvatar = account.ldavatar();
    profile.hdAvatar = account.hdavatar();
    profile.supportVoice = AccountsManager::isSupportVoice(account);
    profile.supportVideo = AccountsManager::isSupportVideo(account);
    profile.privacy = account.has_privacy() ? account.privacy() : kDefaultPrivacy;
    profile.state = account.state();
    profile.features = AccountsManager::getFeatures(account);

    // TODO deprecated
    profile.name = account.name();
    profile.avatar = account.avater();

    context.responseEntity = profile;

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsProfileServiceName,
            "getSingleProfile", (nowInMicro() - dwStartTime), 0);
    return res.result(http::status::ok);
}

void ProfileController::getProfileBatch(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();

    auto& res = context.response;
    auto& account = boost::any_cast<Account&>(context.authResult);
    auto& tokens = boost::any_cast<ContactTokens&>(context.requestEntity);
    const auto& contacts = tokens.contacts;

    if (contacts.empty()) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsProfileServiceName,
                "getProfileBatch", (nowInMicro() - dwStartTime), 1001);
        LOGD << "contacts empty";
        return res.result(http::status::bad_request);
    }

    std::vector<Account> accountList;
    std::vector<std::string> missedUids;
    if (!m_accountsManager->get(contacts, accountList, missedUids)) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsProfileServiceName,
                "getProfileBatch", (nowInMicro() - dwStartTime), 1002);
        return res.result(http::status::internal_server_error);
    }

    ProfileMap profileMap;
    for (auto it = accountList.begin(); it != accountList.end(); ++it) {
        Profile profile;
        if (it->state() == Account::DELETED) {
            profile.state = Account::DELETED;
        } else {
            profile.identityKey = it->identitykey();
            profile.nickname = it->nickname();
            profile.ldAvatar = it->ldavatar();
            profile.hdAvatar = it->hdavatar();
            profile.supportVoice = AccountsManager::isSupportVoice(*it);
            profile.supportVideo = AccountsManager::isSupportVideo(*it);
            profile.privacy = it->has_privacy() ? it->privacy() : kDefaultPrivacy;
            profile.state = it->state();
            profile.features = AccountsManager::getFeatures(*it);

            if (account.uid() == it->uid()) {
                if (!it->encryptnumber().empty()) {
                    profile.encryptNumber = it->encryptnumber();
                    profile.numberPubkey = it->numberpubkey();
                } else {
                    // TODO deprecated
                    profile.phoneNum = it->phonenumber();
                }
            }

            // TODO deprecated
            profile.name = it->name();
            profile.avatar = it->avater();
        }
        profileMap.profileMap[it->uid()] = std::move(profile);
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsProfileServiceName,
        "getProfileBatch", (nowInMicro() - dwStartTime), 0);

    context.responseEntity = profileMap;
    return res.result(http::status::ok);
}

void ProfileController::setName(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto& name = context.pathParams[":name"];
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& statics = context.statics;
    statics.setUid(account->uid());

    auto internalError = [&](const std::string& uid, const std::string& op) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsProfileServiceName,
            "setName", (nowInMicro() - dwStartTime), 1001);
        LOGW << "internal error, uid: " << uid << " op: " << op;
        return res.result(http::status::internal_server_error);
    };

    if (name.empty()) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsProfileServiceName,
            "setName", (nowInMicro() - dwStartTime), 1002);
        LOGI << account->uid() << ":name empty";
        res.result(http::status::bad_request);
        return;
    }
    
    ModifyAccount   mdAccount(account);
    mdAccount.set_name(name);
    auto ret = m_accountsManager->updateAccount(mdAccount);

    if (!ret) {
        return internalError(account->uid(), "update account name");
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsProfileServiceName,
        "setName", (nowInMicro() - dwStartTime), 0);

    res.result(http::status::no_content);
    return;
}

void ProfileController::setPrivacy(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto* privacy = boost::any_cast<Account::Privacy>(&context.requestEntity);
    auto& statics = context.statics;
    statics.setUid(account->uid());

    auto internalError = [&](const std::string& uid, const std::string& op) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsProfileServiceName,
            "setPrivacy", (nowInMicro() - dwStartTime), 1001);
        LOGW << "internal error, uid: " << uid << " op: " << op;
        return res.result(http::status::internal_server_error);
    };
    
    ModifyAccount   mdAccount(account);
    mdAccount.mutable_privacy(*privacy);
    auto ret = m_accountsManager->updateAccount(mdAccount);

    if (!ret) {
        return internalError(account->uid(), "update privacy");
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsProfileServiceName,
        "setPrivacy", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::no_content);
}

void ProfileController::setVersion(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto version = std::stoi(context.pathParams[":version"]);
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& device = AccountsManager::getAuthDevice(*account).get();
    auto& statics = context.statics;
    statics.setUid(account->uid());

    auto internalError = [&](const std::string& uid, const std::string& op) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsProfileServiceName,
            "setVersion", (nowInMicro() - dwStartTime), 1001);
        LOGW << "internal error, uid: " << uid << " op: " << op;
        return res.result(http::status::internal_server_error);
    };
    
    ModifyAccount   mdAccount(account);
    std::shared_ptr<ModifyAccountDevice> mDev = mdAccount.getMutableDevice(device.id());
    mDev->set_version(version);
    auto ret = m_accountsManager->updateDevice(mdAccount, device.id());

    if (!ret) {
        return internalError(account->uid(), "update version");
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsProfileServiceName,
        "setVersion", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::no_content);
}

void ProfileController::setNickname(bcm::HttpContext& context)
{
    static constexpr size_t kMaxNicknameSize = 128;

    auto& res = context.response;
    auto& nickname = context.pathParams[":nickname"];
    auto& account = boost::any_cast<Account&>(context.authResult);

    if (nickname.empty() || nickname.size() > kMaxNicknameSize || !Base64::check(nickname, true)) {
        LOGD << "nickname is invalid: " << nickname;
        return res.result(http::status::bad_request);
    }
    ModifyAccount   mdAccount(&account);
    mdAccount.set_nickname(nickname);
    if (!m_accountsManager->updateAccount(mdAccount)) {
        LOGW << "save nickname failed, uid: " << account.uid();
        return res.result(http::status::internal_server_error);
    }

    return res.result(http::status::no_content);
}

void ProfileController::setAvatar2(bcm::HttpContext& context)
{
    static constexpr size_t kMaxAvatarSize = 2048;

    auto& res = context.response;
    auto& account = boost::any_cast<Account&>(context.authResult);
    auto& bundle = boost::any_cast<AvatarBundle&>(context.requestEntity);

    if (bundle.ldAvatar.size() > kMaxAvatarSize || bundle.hdAvatar.size() > kMaxAvatarSize) {
        LOGD << account.uid() << "size of avatar is invalid";
        return res.result(http::status::bad_request);
    }

    if (!Base64::check(bundle.ldAvatar, true) || !Base64::check(bundle.hdAvatar, true)) {
        LOGD << account.uid() << "avatar string is invalid";
        return res.result(http::status::bad_request);
    }
    
    ModifyAccount   mdAccount(&account);
    mdAccount.set_ldavatar(bundle.ldAvatar);
    mdAccount.set_hdavatar(bundle.hdAvatar);
    if (!m_accountsManager->updateAccount(mdAccount)) {
        LOGW << "save avatar failed, uid: " << account.uid();
        return res.result(http::status::internal_server_error);
    }

    return res.result(http::status::no_content);
}

void ProfileController::setKeys(bcm::HttpContext& context)
{
    static constexpr size_t kMaxKeysSize = 4096;

    auto& res = context.response;
    auto& account = boost::any_cast<Account&>(context.authResult);
    auto& ji = boost::any_cast<JsonInside&>(context.requestEntity);
    auto keysString = ji.j.dump();

    if (keysString.size() > kMaxKeysSize) {
        LOGD << "size of keys is too long" << account.uid();
        return res.result(http::status::bad_request);
    }
    
    ModifyAccount   mdAccount(&account);
    mdAccount.set_profilekeys(Base64::encode(keysString));
    if (!m_accountsManager->updateAccount(mdAccount)) {
        LOGW << "save profile keys failed, uid: " << account.uid();
        return res.result(http::status::internal_server_error);
    }

    return res.result(http::status::no_content);
}

void ProfileController::getKeys(bcm::HttpContext& context)
{
    auto& res = context.response;
    auto& account = boost::any_cast<Account&>(context.authResult);

    if (account.profilekeys().empty()) {
        return res.result(http::status::no_content);
    }

    std::string keysString = Base64::decode(account.profilekeys());
    if (keysString.empty()) {
        return res.result(http::status::no_content);
    }

    JsonInside ji;
    try {
        ji.j = nlohmann::json::parse(keysString);
    } catch (std::exception&) {
        return res.result(http::status::no_content);
    }

    context.responseEntity = ji;
    return res.result(http::status::ok);
}

std::string ProfileController::generateUUID()
{
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    const std::string id = boost::uuids::to_string(uuid);
    const std::string realId = boost::algorithm::replace_all_copy(id, "-", "");
    return realId;
}

} // namespace bcm
