#include "contact_controller.h"
#include "contact_entities.h"
#include <proto/dao/account.pb.h>
#include <utils/log.h>
#include <metrics_client.h>
#include "utils/time.h"
#include "crypto/fnv.h"
#include <sstream>
#include <algorithm>
#include "crypto/base64.h"
#include "crypto/sha1.h"
#include "bloom/bloom_filters.h"
#include "utils/sender_utils.h"
#include "proto/contacts/friend.pb.h"
#include "dispatcher/dispatch_address.h"
#include "utils/account_helper.h"
#include "../store/accounts_manager.h"

namespace bcm {

using namespace metrics;

static constexpr char kMetricsContactServiceName[] = "contacts";
static constexpr uint32_t kMaxFiltersLength = 8*1024*1024;
static constexpr uint32_t kMinFiltersLength = 128;

static const uint64_t kMaxDeltaMillis = 30 * 1000;
static const uint64_t kMaxPayloadSize = 2048;

ContactController::ContactController(std::shared_ptr<dao::Contacts> contacts,
                                     std::shared_ptr<AccountsManager> accountsManager, 
                                     std::shared_ptr<DispatchManager> dispatchMgr,
                                     std::shared_ptr<OfflineDispatcher> offlineDispatcher,
                                     const SizeCheckConfig& cfg)
    : m_contacts(std::move(contacts))
    , m_accounts(std::move(accountsManager))
    , m_dispatchMgr(std::move(dispatchMgr))
    , m_offlineDispatcher(std::move(offlineDispatcher))
    , m_sizeCheckConfig(cfg)
{
}

void ContactController::addRoutes(HttpRouter& router)
{
    // new
    router.add(http::verb::put, "/v2/contacts/parts", Authenticator::AUTHTYPE_ALLOW_MASTER,
            std::bind(&ContactController::storeInParts, shared_from_this(), std::placeholders::_1),
            new JsonSerializerImp<ContactInParts>(), nullptr);

    router.add(http::verb::post, "/v2/contacts/parts", Authenticator::AUTHTYPE_ALLOW_ALL,
            std::bind(&ContactController::restoreInParts, shared_from_this(), std::placeholders::_1),
            new JsonSerializerImp<ContactInPartsReq>(), new JsonSerializerImp<ContactInParts>());

    // contacts bloom filters
    router.add(http::verb::put, "/v2/contacts/filters", Authenticator::AUTHTYPE_ALLOW_MASTER,
            std::bind(&ContactController::putContactsFilters, shared_from_this(), std::placeholders::_1),
            new JsonSerializerImp<ContactsFiltersPutReq>(), new JsonSerializerImp<ContactsFiltersVersion>());

    router.add(http::verb::patch, "/v2/contacts/filters", Authenticator::AUTHTYPE_ALLOW_MASTER,
            std::bind(&ContactController::patchContactsFilters, shared_from_this(), std::placeholders::_1),
            new JsonSerializerImp<ContactsFiltersPatchReq>(), new JsonSerializerImp<ContactsFiltersVersion>());

    router.add(http::verb::delete_, "/v2/contacts/filters", Authenticator::AUTHTYPE_ALLOW_MASTER,
            std::bind(&ContactController::deleteContactsFilters, shared_from_this(), std::placeholders::_1),
            nullptr, nullptr);

    // friend
    router.add(http::verb::put, "/v2/contacts/friends/request", Authenticator::AUTHTYPE_ALLOW_MASTER,
            std::bind(&ContactController::friendRequest, shared_from_this(),
                    std::placeholders::_1),
            new JsonSerializerImp<FriendRequestParams>(), nullptr);

    router.add(http::verb::put, "/v2/contacts/friends/reply", Authenticator::AUTHTYPE_ALLOW_MASTER,
            std::bind(&ContactController::friendReply, shared_from_this(),
                    std::placeholders::_1),
            new JsonSerializerImp<FriendReplyParams>(), nullptr);

    router.add(http::verb::delete_, "/v2/contacts/friends", Authenticator::AUTHTYPE_ALLOW_MASTER,
            std::bind(&ContactController::deleteFriend, shared_from_this(),
                    std::placeholders::_1),
            new JsonSerializerImp<DeleteFriendParams>(), nullptr);
}

void ContactController::storeInParts(HttpContext& context)
{
    const static int BEGIN_PARTS_INDEX = 0;
    const static int END_PARTS_INDEX = 19;

    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto& req = context.request;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto* contactsInPart = boost::any_cast<ContactInParts>(&context.requestEntity);

    LOGI << "uid: " << account->uid() << " store in parts " << jsonable::toPrintable(*contactsInPart);

    size_t total = 0;
    for (auto& part : contactsInPart->contacts) {
        int partNum = std::stoi(part.first);
        if (partNum < BEGIN_PARTS_INDEX || partNum > END_PARTS_INDEX) {
            LOGE << "uid: " << account->uid() << " error part num:" << partNum;
            return res.result(http::status::bad_request);
        }
        total += part.second.size();
    }

    if (total > m_sizeCheckConfig.contactsSize) {
        LOGE << "contacts size " << total << " is more than the limit " << m_sizeCheckConfig.contactsSize;
        return res.result(http::status::bad_request);
    }

    if (contactsInPart->contacts.empty()) {
        LOGE << "uid:" << account->uid() << " error request";
        return res.result(http::status::bad_request);
    }

    auto ec = m_contacts->setInParts(account->uid(), contactsInPart->contacts);

    if (ec != dao::ERRORCODE_SUCCESS) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsContactServiceName,
            "storeInParts", (nowInMicro() - dwStartTime), 1001);
        LOGE << "storeInParts contact failed, uid: " << account->uid() << " size: " << req.body().size();
        return res.result(http::status::internal_server_error);
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsContactServiceName,
        "storeInParts", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::ok);
}

void ContactController::restoreInParts(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto& req = context.request;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto* contactsInPartReq = boost::any_cast<ContactInPartsReq>(&context.requestEntity);

    std::vector<std::string> parts;
    for (auto& reqg : contactsInPartReq->hash) {
        parts.push_back(reqg.first);
    }

    LOGI << "uid: " << account->uid() << jsonable::toPrintable(*contactsInPartReq);

    if (parts.empty()) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsContactServiceName,
            "restoreInParts", (nowInMicro() - dwStartTime), 1002);
        LOGD << "not found parts in request, uid: " << account->uid();
        return res.result(http::status::bad_request);
    }

    std::map<std::string, std::string> latestContacts;
    auto ec = m_contacts->getInParts(account->uid(), parts, latestContacts);

    if (ec == dao::ERRORCODE_NO_SUCH_DATA) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsContactServiceName,
            "restoreInParts", (nowInMicro() - dwStartTime), 1000);
        LOGE << "restoreInParts contact not found data, uid: " << account->uid() << " size: " << req.body().size();
        return res.result(http::status::no_content);
    }

    if (ec != dao::ERRORCODE_SUCCESS) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsContactServiceName,
            "restoreInParts", (nowInMicro() - dwStartTime), 1000);
        LOGE << "restoreInParts contact failed, uid: " << account->uid() << " size: " << req.body().size();
        return res.result(http::status::internal_server_error);
    }

    if (latestContacts.size() != parts.size()) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsContactServiceName,
            "restoreInParts", (nowInMicro() - dwStartTime), 1001);
        LOGE << "restoreInParts contact not exist before, uid: " << account->uid();
        return res.result(http::status::no_content);    
    }

    // response with diff contact
    ContactInParts contactInParts;
    for (auto &part : parts) {
        uint32_t latestHash = FNV::hash(latestContacts[part].c_str(), latestContacts[part].size());
        if (latestHash != contactsInPartReq->hash[part]) {
            LOGI << "uid: " << account->uid() << " part " << part << " changes, before: " 
                << contactsInPartReq->hash[part] << ", now: " << latestHash;
            contactInParts.contacts[part] = latestContacts[part];
        }     
    }
    context.responseEntity = contactInParts;

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsContactServiceName,
        "storeInParts", (nowInMicro() - dwStartTime), 0);

    return res.result(http::status::ok);
}

void ContactController::putContactsFilters(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    context.statics.setUid(account->uid());

    auto* filtersReq = boost::any_cast<ContactsFiltersPutReq>(&context.requestEntity);
    auto realContent = Base64::decode(filtersReq->content);

    auto markMetrics = [=](int code){
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsContactServiceName,
                "putContactsFilters", (nowInMicro() - dwStartTime), code);
    };

    // check req
    if (realContent.empty()) {
        markMetrics(400);
        LOGE << account->uid() << ": " << "filters empty";
        return res.result(http::status::bad_request);
    }
    auto filtersLength = realContent.size();
    if (filtersLength < kMinFiltersLength || filtersLength > kMaxFiltersLength) {
        markMetrics(400);
        LOGE << account->uid() << ": " << "length incorrect: " << filtersLength;
        return res.result(http::status::bad_request);
    }
    if (filtersReq->algo != 0) {
        markMetrics(400);
        LOGE << account->uid() << ": " << "unsupported algo: " << filtersReq->algo;
        return res.result(http::status::bad_request);
    }

    ContactsFiltersVersion version;
    version.version = Base64::encode(SHA1::digest(filtersReq->content
            + std::to_string(filtersReq->algo)))
        + std::to_string(nowInMilli());
    
    ModifyAccount   mdAccount(account);
    mdAccount.clear_contactsfilters();
    auto filters = mdAccount.getMutableContactsFilters();
    filters->set_algo(filtersReq->algo);
    filters->set_content(filtersReq->content);
    filters->set_version(version.version);

    bool upRet = m_accounts->updateAccount(mdAccount);

    int ret = 0;
    if (upRet) {
        res.result(http::status::ok);
        context.responseEntity = version;
        ret = 200;
    } else {
        res.result(http::status::internal_server_error);
        ret = 500;
    }

    markMetrics(ret);
}

void ContactController::patchContactsFilters(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto account = boost::any_cast<Account>(&context.authResult);

    context.statics.setUid(account->uid());

    auto markMetrics = [=](int code){
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsContactServiceName,
                "patchContactsFilters", (nowInMicro() - dwStartTime), code);
    };

    auto filtersPatch = boost::any_cast<ContactsFiltersPatchReq>(&context.requestEntity);

    auto filtersContent = account->mutable_contactsfilters();
    if (filtersPatch->version != filtersContent->version()) {
        LOGE << account->uid() << ": " << "version mismatch: " << filtersPatch->version << ": " << filtersContent->version();
        markMetrics(409);
        return res.result(http::status::conflict);
    }

    auto realContent = Base64::decode(filtersContent->content());
    BloomFilters filters(filtersContent->algo(), realContent);

    std::map<uint32_t, bool> mValues;

    for (const auto& i : filtersPatch->patches) {
        mValues[i.position] = i.value;
    }

    filters.update(mValues);
    
    ModifyAccount   mdAccount(account);
    auto modifyFilters = mdAccount.getMutableContactsFilters();
    
    modifyFilters->set_content(Base64::encode(filters.getFiltersContent()));
    ContactsFiltersVersion version;
    version.version = Base64::encode(SHA1::digest(filtersContent->content()
            + std::to_string(filtersContent->algo())))
        + std::to_string(nowInMilli());
    modifyFilters->set_version(version.version);

    bool upRet = m_accounts->updateAccount(mdAccount);

    int ret = 0;
    if (upRet) {
        res.result(http::status::ok);
        context.responseEntity = version;
        ret = 200;
    } else {
        res.result(http::status::internal_server_error);
        ret = 500;
    }
    markMetrics(ret);
}

void ContactController::deleteContactsFilters(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    context.statics.setUid(account->uid());
    
    ModifyAccount   mdAccount(account);
    mdAccount.clear_contactsfilters();

    bool upRet = m_accounts->updateAccount(mdAccount);

    int ret = 0;
    if (upRet) {
        res.result(http::status::no_content);
        ret = 204;
    } else {
        res.result(http::status::internal_server_error);
        ret = 500;
    }

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsContactServiceName,
        "deletecontactsFilters", (nowInMicro() - dwStartTime), ret);
}

void ContactController::friendRequest(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsContactServiceName, 
            "friendRequest");
    Account* account = boost::any_cast<Account>(&context.authResult);
    FriendRequestParams* params = 
            boost::any_cast<FriendRequestParams>(&context.requestEntity);
    auto& response = context.response;
    
    LOGT << "friend request received, uid: " << account->uid() 
         << ", target: " << params->target;

    uint64_t currentMillis = nowInMilli();
    uint64_t deltaMillis = (currentMillis > params->timestamp) 
            ? (currentMillis - params->timestamp) 
            : (params->timestamp - currentMillis);

    if (deltaMillis > kMaxDeltaMillis) {
        LOGE << "diff of friend request time " << params->timestamp 
             << " and server time " << currentMillis << " is greater than " 
             << kMaxDeltaMillis << " millis, uid: " << account->uid() 
             << ", target: " << params->target;
        marker.setReturnCode(1001);
        response.result(http::status::bad_request);
        return;
    }

    if (params->payload.size() > kMaxPayloadSize) {
        LOGE << "payload size is greater than " << kMaxPayloadSize 
             << " bytes, uid: " << account->uid() 
             << ", target: " << params->target;
        marker.setReturnCode(1002);
        response.result(http::status::bad_request);
        return;
    }

    std::string toVerify;
    toVerify.append(account->uid())
            .append(params->target)
            .append(std::to_string(params->timestamp))
            .append(params->payload)
            .append("/v2/contacts/friends/request");
    if (!AccountHelper::verifySignature(account->publickey(), 
                                        toVerify, params->signature)) {
        LOGE << "invalid signature: " << params->signature 
             << ", toVerify: " << toVerify 
             << ", pubkey: " << account->publickey();
        marker.setReturnCode(1010);
        response.result(http::status::not_acceptable);
        return;
    }

    if (account->uid() == params->target) {
        LOGE << "attempt to add self, uid: " << account->uid();
        marker.setReturnCode(1008);
        response.result(http::status::bad_request);
        return;
    }

    Account targetAccount;
    uint32_t retval = getValidAccount(params->target, targetAccount);
    if (retval != uint32_t(http::status::ok)) {
        marker.setReturnCode(1003);
        response.result(http::status(retval));
        return;
    }

    std::string proposerUid = 
            encryptUidWithAccount(account->uid(), targetAccount);
    if (proposerUid.empty()) {
        marker.setReturnCode(1006);
        response.result(http::status::internal_server_error);
        return;
    }

    FriendMessage friendMsg;
    FriendRequest* friendReq = friendMsg.add_requests();
    if (friendReq == nullptr) {
        LOGE << "failed to allocate FriendRequest, uid: " << account->uid() 
             << ", target: " << params->target;
        marker.setReturnCode(1004);
        response.result(http::status::internal_server_error);
        return;
    }

    friendReq->set_proposer(proposerUid);
    friendReq->set_timestamp(params->timestamp);
    friendReq->set_payload(params->payload);
    friendReq->set_signature(params->signature);

    PubSubMessage msg;
    msg.set_type(PubSubMessage::FRIEND);
    msg.set_content(friendMsg.SerializeAsString());

    std::string msgStr = msg.SerializeAsString();
    bool published = false;
    for (int i = 0; i < targetAccount.devices_size(); i++) {
        const Device& device = targetAccount.devices(i);
        DispatchAddress address(targetAccount.uid(), device.id());
        if (m_dispatchMgr->publish(address, msgStr)) {
            LOGD << "successfully publish friend request message to " << address;
            published = true;
        }
    }

    if (!published) {
        int64_t eventId = 0;
        dao::ErrorCode ec = m_contacts->addFriendEvent(targetAccount.uid(), 
                dao::FriendEventType::FRIEND_REQUEST, 
                friendReq->SerializeAsString(), eventId);
        if (ec != dao::ERRORCODE_SUCCESS) {
            LOGE << "failed to add friend request, target: " 
                 << targetAccount.uid() << ", proposer: " << account->uid() 
                 << ", ec: " << ec;
            marker.setReturnCode(1005);
            response.result(http::status::internal_server_error);
            return;
        }

        push::Notification notification;
        notification.friendReq(proposerUid, params->payload);

        for (int i = 0; i < targetAccount.devices_size(); i++) {
            const Device& device = targetAccount.devices(i);
            if (!AccountsManager::isDevicePushable(device)) {
                continue;
            }

            DispatchAddress address(targetAccount.uid(), device.id());
            notification.setTargetAddress(address);
            notification.setDeviceInfo(device);
            std::string pushType = notification.getPushType();
            if (!pushType.empty()) {
                m_offlineDispatcher->dispatch(pushType, notification);
            }
        }

    }

    response.result(http::status::ok);
}

void ContactController::friendReply(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsContactServiceName, 
            "friendReply");
    Account* account = boost::any_cast<Account>(&context.authResult);
    FriendReplyParams* params = 
            boost::any_cast<FriendReplyParams>(&context.requestEntity);
    auto& response = context.response;

    LOGT << "friend reply received, uid: " << account->uid() 
         << ", proposer: " << params->proposer << ", approved: " 
         << params->approved;
    
    if (params->payload.size() > kMaxPayloadSize) {
        LOGE << "payload size is greater than " << kMaxPayloadSize 
             << " bytes, uid: " << account->uid() 
             << ", proposer: " << params->proposer;
        marker.setReturnCode(1001);
        response.result(http::status::bad_request);
        return;
    }

    std::string toVerify;
    toVerify.append(account->uid())
            .append(params->approved ? "true" : "false")
            .append(params->proposer)
            .append(std::to_string(params->timestamp))
            .append(params->payload)
            .append(params->requestSignature)
            .append("/v2/contacts/friends/reply");
    if (!AccountHelper::verifySignature(account->publickey(), 
                                        toVerify, params->signature)) {
        LOGE << "invalid signature: " << params->signature 
             << ", toVerify: " << toVerify 
             << ", pubkey: " << account->publickey();
        marker.setReturnCode(1010);
        response.result(http::status::not_acceptable);
        return;
    }

    Account proposerAccount;
    uint32_t retval = getValidAccount(params->proposer, proposerAccount);
    if (retval != uint32_t(http::status::ok)) {
        marker.setReturnCode(1002);
        response.result(http::status(retval));
        return;
    }

    std::string targetUid = 
            encryptUidWithAccount(account->uid(), proposerAccount);
    if (targetUid.empty()) {
        marker.setReturnCode(1003);
        response.result(http::status::internal_server_error);
        return;
    }

    FriendMessage friendMsg;
    FriendReply* friendReply = friendMsg.add_replies();
    if (friendReply == nullptr) {
        LOGE << "failed to allocate FriendReply, uid: " << account->uid() 
             << ", proposer: " << params->proposer;
        marker.setReturnCode(1004);
        response.result(http::status::internal_server_error);
        return;
    }

    friendReply->set_approved(params->approved);
    friendReply->set_target(targetUid);
    friendReply->set_timestamp(params->timestamp);
    friendReply->set_payload(params->payload);
    friendReply->set_signature(params->signature);
    friendReply->set_requestsignature(params->requestSignature);

    PubSubMessage msg;
    msg.set_type(PubSubMessage::FRIEND);
    msg.set_content(friendMsg.SerializeAsString());

    std::string msgStr = msg.SerializeAsString();
    bool published = false;
    for (int i = 0; i < proposerAccount.devices_size(); i++) {
        const Device& device = proposerAccount.devices(i);
        DispatchAddress address(proposerAccount.uid(), device.id());
        if (m_dispatchMgr->publish(address, msgStr)) {
            LOGD << "successfully publish friend reply message to " << address;
            published = true;
        }
    }

    if (!published) {
        int64_t eventId = 0;
        dao::ErrorCode ec = m_contacts->addFriendEvent(proposerAccount.uid(),
                dao::FriendEventType::FRIEND_REPLY, 
                friendReply->SerializeAsString(), eventId);
        if (ec != dao::ERRORCODE_SUCCESS) {
            LOGE << "failed to add friend reply, proposer: " 
                 << proposerAccount.uid() << ", target: " << account->uid();
            marker.setReturnCode(1005);
            response.result(http::status::internal_server_error);
            return;
        }
    }

    response.result(http::status::ok);
}

void ContactController::deleteFriend(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsContactServiceName, 
            "deleteFriend");
    Account* account = boost::any_cast<Account>(&context.authResult);
    DeleteFriendParams* params = 
            boost::any_cast<DeleteFriendParams>(&context.requestEntity);
    auto& response = context.response;

    LOGT << "friend delete received, uid: " << account->uid() 
         << ", target: " << params->target;

    uint64_t currentMillis = nowInMilli();
    uint64_t deltaMillis = (currentMillis > params->timestamp) 
            ? (currentMillis - params->timestamp) 
            : (params->timestamp - currentMillis);

    if (deltaMillis > kMaxDeltaMillis) {
        LOGE << "diff of friend delete time " << params->timestamp 
             << " and server time  " << currentMillis << " is greater than " 
             << kMaxDeltaMillis << " millis, uid: " << account->uid() 
             << ", target: " << params->target;
        marker.setReturnCode(1001);
        response.result(http::status::bad_request);
        return;
    }

    std::string toVerify;
    toVerify.append(account->uid())
            .append(params->target)
            .append(std::to_string(params->timestamp))
            .append("/v2/contacts/friends");
    if (!AccountHelper::verifySignature(account->publickey(), 
                                        toVerify, params->signature)) {
        LOGE << "invalid signature: " << params->signature 
             << ", toVerify: " << toVerify 
             << ", pubkey: " << account->publickey();
        marker.setReturnCode(1010);
        response.result(http::status::not_acceptable);
        return;
    }
    
    Account targetAccount;
    uint32_t retval = getValidAccount(params->target, targetAccount);
    if (retval != uint32_t(http::status::ok)) {
        marker.setReturnCode(1002);
        response.result(http::status(retval));
        return;
    }

    std::string proposerUid = 
            encryptUidWithAccount(account->uid(), targetAccount);
    if (proposerUid.empty()) {
        marker.setReturnCode(1003);
        response.result(http::status::internal_server_error);
        return;
    }

    FriendMessage friendMsg;
    DeleteFriend* delFriend = friendMsg.add_deletes();
    if (delFriend == nullptr) {
        LOGE << "failed to allocate DeleteFriend, uid: " << account->uid() 
             << ", target: " << params->target;
        marker.setReturnCode(1004);
        response.result(http::status::internal_server_error);
        return;
    }

    delFriend->set_proposer(proposerUid);
    delFriend->set_timestamp(params->timestamp);
    delFriend->set_signature(params->signature);

    PubSubMessage msg;
    msg.set_type(PubSubMessage::FRIEND);
    msg.set_content(friendMsg.SerializeAsString());

    std::string msgStr = msg.SerializeAsString();
    bool published = false;
    for (int i = 0; i < targetAccount.devices_size(); i++) {
        const Device& device = targetAccount.devices(i);
        DispatchAddress address(targetAccount.uid(), device.id());
        if (m_dispatchMgr->publish(address, msgStr)) {
            LOGD << "successfully publish friend delete message to " << address;
            published = true;
        }
    }

    if (!published) {
        int64_t eventId = 0;
        dao::ErrorCode ec = m_contacts->addFriendEvent(targetAccount.uid(), 
                dao::FriendEventType::DELETE_FRIEND, 
                delFriend->SerializeAsString(), eventId);
        if (ec != dao::ERRORCODE_SUCCESS) {
            LOGE << "failed to add friend deletion, target: " 
                 << targetAccount.uid() << ", proposer: " << account->uid();
            marker.setReturnCode(1005);
            response.result(http::status::internal_server_error);
            return;
        }
    }

    response.result(http::status::ok);
}

uint32_t ContactController::getValidAccount(const std::string& uid, 
                                            Account& account) {
    dao::ErrorCode ec = m_accounts->get(uid, account);
    if (dao::ERRORCODE_NO_SUCH_DATA == ec) {
        LOGE << "could not found user with uid " << uid;
        return uint32_t(http::status::not_found);
    }

    if (dao::ERRORCODE_SUCCESS != ec) {
        LOGE << "failed to get account with uid: " << uid << ":" << ec;
        return uint32_t(http::status::internal_server_error);
    }

    if (account.state() == Account::DELETED) {
        LOGE << "account with uid " << uid << " is destoryed";
        return uint32_t(http::status::not_found);
    }

    return uint32_t(http::status::ok);
}

std::string ContactController::encryptUidWithAccount(const std::string& uid, 
                                                     const Account& account) {
    uint32_t version;
    std::string iv;
    std::string ephemeralPubkey;
    std::string proposerUid;

    int retval = SenderUtils::encryptSender(uid, account.publickey(), version, 
                                            iv, ephemeralPubkey, proposerUid);
    if (retval != 0) {
        LOGE << "failed to encrypt uid: " << uid << ", with account: " 
             << account.uid() << ", pubkey: " << account.publickey() 
             << ", error: " << retval;
        return "";
    }

    nlohmann::json j = nlohmann::json::object({
            {"version", version},
            {"ephemeralPubkey", Base64::encode(ephemeralPubkey)},
            {"iv", Base64::encode(iv)},
            {"source", Base64::encode(proposerUid)}
    });

    return Base64::encode(j.dump());
}



} // namespace bcm
