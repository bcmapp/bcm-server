#include "features/bcm_features.h"
#include "proto/dao/pending_group_user.pb.h"
#include "group_manager_controller.h"
#include "group_common_entities.h"
#include "group_manager_entities.h"
#include "utils/log.h"
#include "utils/time.h"
#include "utils/account_helper.h"
#include "dao/client.h"
#include "crypto/sha1.h"
#include "group/group_event.h"
#include "proto/dao/account.pb.h"
#include "proto/dao/error_code.pb.h"
#include "proto/group/message.pb.h"
#include <metrics_client.h>
#include "config/group_store_format.h"

#include "redis/redis_manager.h"
#include "redis/online_redis_manager.h"
#include "redis/reply.h"
#include "store/accounts_manager.h"
#include "crypto/base64.h"
#include "http/custom_http_status.h"
#include "bloom/bloom_filters.h"
#include "limiters/limiter_manager.h"
#include "limiters/distributed_limiter.h"
#include "limiters/dependency_limiter.h"
#include "crypto/hex_encoder.h"

namespace bcm {
namespace http = boost::beast::http;

using namespace metrics;

static constexpr char kMetricsGmanagerServiceName[] = "group_manager";
static const std::string kGroupCreationLimiterName = "GroupCreationLimiter";
static const std::string kGroupCreationConfigKey = "special/group_creation";
static const std::string kGroupFireKeysUpdateLimiterName = "GroupKeysUpdateLimiter";
static const std::string kGroupFireKeysUpdateConfigKey = "special/group_keys_update";
static const std::string kDhKeysLimiterName = "DhKeysLimiter";
static const std::string kDhKeysConfigKey = "special/dh_keys";
static const std::string kGroupMemberJoinLimiterName = "GroupMemberJoinLimiter";
static const std::string kGroupMemberJoinLimiterConfigKey = "special/group_member_join";
constexpr int64_t kKeysCacheTtl = 600;


GroupManagerController::GroupManagerController(std::shared_ptr<bcm::GroupMsgService> groupMsgService,
                                               std::shared_ptr<bcm::AccountsManager> accountsManager,
                                               std::shared_ptr<bcm::DispatchManager> dispatchManager,
                                               const MultiDeviceConfig& multiDeviceCfg,
                                               const GroupConfig& groupConfig)
    : m_groups(dao::ClientFactory::groups())
    , m_groupUsers(dao::ClientFactory::groupUsers())
    , m_groupKeys(dao::ClientFactory::groupKeys())
    , m_groupMessage(dao::ClientFactory::groupMsgs())
    , m_pendingGroupUsers(dao::ClientFactory::pendingGroupUsers())
    , m_qrCodeGroupUsers(dao::ClientFactory::qrCodeGroupUsers())
    , m_groupMsgService(groupMsgService)
    , m_accountsManager(accountsManager)
    , m_dispatchManager(dispatchManager)
    , m_multiDeviceConfig(multiDeviceCfg)
    , m_groupConfig(groupConfig)
    , m_keysCache(kKeysCacheTtl)
{
    // m_groupCreationLimiter
    auto ptr = LimiterManager::getInstance()->find(kGroupCreationLimiterName);
    if (ptr == nullptr) {
        std::shared_ptr<DistributedLimiter> limiter(new DistributedLimiter(kGroupCreationLimiterName, 
                                                                           kGroupCreationConfigKey, 
                                                                           1000 * 3600 * 24,
                                                                           20));
        auto it = LimiterManager::getInstance()->emplace(limiter->identity(), limiter);
        LimiterConfigurationManager::getInstance()->registerObserver(limiter);
        ptr = it.first;
    }
    m_groupCreationLimiter = ptr;

    // m_groupFireKeysUpdateLimiter
    auto update_ptr = LimiterManager::getInstance()->find(kGroupFireKeysUpdateLimiterName);
    if (update_ptr == nullptr) {
        std::shared_ptr<DistributedLimiter> limiter(new DistributedLimiter(kGroupFireKeysUpdateLimiterName,
                                                                           kGroupFireKeysUpdateConfigKey,
                                                                           1000 * 3600 * 24,
                                                                           50));
        auto it = LimiterManager::getInstance()->emplace(limiter->identity(), limiter);
        LimiterConfigurationManager::getInstance()->registerObserver(limiter);
        update_ptr = it.first;
    }
    m_groupFireKeysUpdateLimiter = update_ptr;

    auto dhKeysLimiter = LimiterManager::getInstance()->find(kDhKeysLimiterName);
    if (dhKeysLimiter == nullptr) {
        std::shared_ptr<DistributedLimiter> lower(new DistributedLimiter(kDhKeysLimiterName, 
                                                                         kDhKeysConfigKey, 
                                                                         1000 * 3600 * 24,
                                                                         20));
        std::vector<std::shared_ptr<ILimiter>> dependencies;
        dependencies.emplace_back(m_groupCreationLimiter);
        std::shared_ptr<DependencyLimiter> limiter(new DependencyLimiter(lower, dependencies));
        auto it = LimiterManager::getInstance()->emplace(limiter->identity(), limiter);
        LimiterConfigurationManager::getInstance()->registerObserver(lower);
        dhKeysLimiter = it.first;
    }
    m_dhKeysLimiter = dhKeysLimiter;

    auto groupMemberJoinLimiter = LimiterManager::getInstance()->find(kGroupMemberJoinLimiterName);
    if (groupMemberJoinLimiter == nullptr) {
        std::shared_ptr<DistributedLimiter> limiter(new DistributedLimiter(kGroupMemberJoinLimiterName,
                                                                           kGroupMemberJoinLimiterConfigKey,
                                                                           1000 * 3600 * 24,
                                                                           30));
        auto it = LimiterManager::getInstance()->emplace(limiter->identity(), limiter);
        LimiterConfigurationManager::getInstance()->registerObserver(limiter);
        groupMemberJoinLimiter = it.first;
    }
    m_groupMemberJoinLimiter = groupMemberJoinLimiter;
}

void GroupManagerController::addRoutes(bcm::HttpRouter& router)
{
    router.add(http::verb::put, "/v1/group/deliver/create", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onCreateGroup, shared_from_this(), std::placeholders::_1));

    router.add(http::verb::put, "/v1/group/deliver/update", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onUpdateGroupInfo, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<UpdateGroupInfoBody>, new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::put, "/v1/group/deliver/invite", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onInviteGroupMember, shared_from_this(), std::placeholders::_1));

    router.add(http::verb::put, "/v1/group/deliver/kick", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onKickGroupMember, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<KickGroupMemberBody>, new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::put, "/v1/group/deliver/leave", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onLeaveGroup, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<LeaveGroupBody>, new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::put, "/v1/group/deliver/query_info", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&GroupManagerController::onQueryGroupInfo, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<QueryGroupInfoBody>, new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::post, "/v1/group/deliver/query_info_batch", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&GroupManagerController::onQueryGroupInfoBatch, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<QueryGroupInfoByGidBatch>, new JsonSerializerImp<GroupResponse>);

    // TODO: deprecated
    router.add(http::verb::put, "/v1/group/deliver/query_joined_list", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&GroupManagerController::onQueryJoinGroupList, shared_from_this(), std::placeholders::_1),
               nullptr, new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::put, "/v1/group/deliver/query_member_list", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&GroupManagerController::onQueryGroupMemberList, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<QueryGroupMemberList>, new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::post, "/v1/group/deliver/query_member_list_segment", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&GroupManagerController::onQueryGroupMemberListSegment, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<QueryGroupMemberListSeg>, new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::put, "/v1/group/deliver/query_member", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&GroupManagerController::onQueryGroupMemberInfo, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<QueryGroupMemberInfo>, new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::put, "/v1/group/deliver/update_user", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onUpdateGroupUserInfo, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<UpdateGroupUserBody>, new JsonSerializerImp<GroupResponse>);

    // TODO: deprecated
    router.add(http::verb::put, "/v1/group/deliver/update_notice", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onUpdateGroupNotice, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<UpdateGroupBody>, new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::put,
               "/v1/group/deliver/join_group_by_code",
               Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onGroupJoinRequst, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<GroupJoinRequest>,
               new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::post,
               "/v1/group/deliver/query_group_pending_list",
               Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onQueryGroupPendingList, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<QueryGroupPendingListRequest>,
               new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::put,
               "/v1/group/deliver/review_join_request",
               Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onReviewJoinRequest, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<ReviewJoinResultList>,
               new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::put,
               "/v1/group/deliver/upload_password",
               Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onUploadPassword, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<ReviewJoinResultList>,
               new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::post, "/v1/group/deliver/get_owner_confirm", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onGetOwnerConfirm, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<GetOwnerConfirmInfo>, new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::post, "/v1/group/deliver/is_qr_code_valid", Authenticator::AUTHTYPE_NO_AUTH,
               std::bind(&GroupManagerController::onIsQrCodeValid, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<QrCodeInfo>, new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::put, "/v2/group/deliver/invite", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onInviteGroupMemberV2, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<InviteGroupMemberBodyV2>, new JsonSerializerImp<GroupResponse>);

    // TODO: deprecated
    router.add(http::verb::put, "/v2/group/deliver/create", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onCreateGroupV2, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<CreateGroupBodyV2>, new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::put,
               "/v2/group/deliver/update",
               Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onUpdateGroupInfoV2, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<UpdateGroupInfoBodyV2>,
               new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::put,
               "/v3/group/deliver/update",
               Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onUpdateGroupInfoV3, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<UpdateGroupInfoBodyV3>,
               new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::post,
               "/v1/group/deliver/member_list_ordered",
               Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&GroupManagerController::onQueryMemberListOrdered, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<QueryMemberListOrderedBody>,
               new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::put, "/v1/group/extension", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onSetGroupExtensionInfo, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<GroupExtensionInfo>, new JsonSerializerImp<GroupResponse>);

    router.add(http::verb::post, "/v1/group/extension", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&GroupManagerController::onGetGroupExtensionInfo, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<QueryGroupExtensionInfo>, new JsonSerializerImp<GroupExtensionInfo>);

    // V3
    router.add(http::verb::put, "/v3/group/deliver/create", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onCreateGroupV3, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<CreateGroupBodyV3>, new JsonSerializerImp<CreateGroupBodyV3Resp>);

    router.add(http::verb::post, "/v3/group/deliver/group_keys", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&GroupManagerController::onFetchGroupKeysV3, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<FetchGroupKeysRequest>, new JsonSerializerImp<FetchGroupKeysResponse>);

    router.add(http::verb::post, "/v3/group/deliver/latest_group_keys", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&GroupManagerController::onFetchLatestGroupKeysV3, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<FetchLatestGroupKeysRequest>, new JsonSerializerImp<FetchLatestGroupKeysResponse>);

    router.add(http::verb::put, "/v3/group/deliver/kick", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onKickGroupMemberV3, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<KickGroupMemberBody>);

    router.add(http::verb::put, "/v3/group/deliver/leave", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onLeaveGroupV3, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<LeaveGroupBody>);

    router.add(http::verb::post, "/v3/group/deliver/fire_group_keys_update", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::fireGroupKeysUpdateV3, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<FireGroupKeysUpdateRequest>, new JsonSerializerImp<FireGroupKeysUpdateResponse>);

    router.add(http::verb::put, "/v3/group/deliver/group_keys_update", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onUploadGroupKeysV3, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<UploadGroupKeysRequest>);

    router.add(http::verb::post, "v3/group/deliver/dh_keys", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onDhKeysV3, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<DhKeysRequest>, new JsonSerializerImp<DhKeysResponse>);

    router.add(http::verb::post, "/v3/group/deliver/prepare_key_update", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onPrepareKeyUpdateV3, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<PrepareKeyUpdateRequestV3>, new JsonSerializerImp<PrepareKeyUpdateResponseV3>);

    router.add(http::verb::put,
               "/v3/group/deliver/join_group_by_code",
               Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onGroupJoinRequstV3, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<GroupJoinRequestV3>,
               new JsonSerializerImp<GroupJoinResponseV3>);

    router.add(http::verb::put,
               "/v3/group/deliver/add_me",
               Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onAddMeRequstV3, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<AddMeRequestV3>);

    router.add(http::verb::put, "/v3/group/deliver/invite", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onInviteGroupMemberV3, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<InviteGroupMemberRequestV3>, new JsonSerializerImp<InviteGroupMemberResponseV3>);

    router.add(http::verb::put,
               "/v3/group/deliver/review_join_request",
               Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&GroupManagerController::onReviewJoinRequestV3, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<ReviewJoinResultRequestV3>);

    router.add(http::verb::post,
               "/v3/group/deliver/members",
               Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&GroupManagerController::onQueryMembersV3, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<QueryMembersRequestV3>, 
               new JsonSerializerImp<QueryMembersResponseV3>);
}

void GroupManagerController::onCreateGroup(HttpContext& context)
{
    context.response.result(static_cast<unsigned>(bcm::custom_http_status::upgrade_requried));
    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName,
                                                         "onCreateGroup",
                                                         0, 
                                                         context.response.result_int());
    return;
}

void GroupManagerController::onUpdateGroupInfo(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    res.result(http::status::ok);

    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& statics = context.statics;
    statics.setUid(account->uid());

    auto* updateGroupBody = boost::any_cast<UpdateGroupInfoBody>(&context.requestEntity);
    uint64_t gid = updateGroupBody->gid;
    GroupResponse response;

    auto errorResponse = [&]() {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onUpdateGroupInfo",
            (nowInMicro() - dwStartTime), response.code);
        LOGW << "error, uid: " << account->uid() << " code: " << response.code << " msg:" << response.msg;
        context.responseEntity = response;
    };

    auto internalError = [&](const std::string& op) {
        LOGW << "internal error, uid: " << account->uid() << " op: " << op;
        response.code = group::ERRORCODE_INTERNAL_ERROR;
        response.msg = "internal error";
        context.responseEntity = response;
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onUpdateGroupInfo",
            (nowInMicro() - dwStartTime), group::ERRORCODE_INTERNAL_ERROR);
    };

    bool checked = updateGroupBody->check(response);
    if (!checked) {
        return errorResponse();
    }

    GroupUser::Role role;
    auto roleRet = m_groupUsers->getMemberRole(gid, account->uid(), role);

    LOGD << "get member role: " << account->uid() << ": " << gid << ": " << roleRet << ": " << role;

    if (roleRet != dao::ERRORCODE_SUCCESS) {
        if (roleRet == dao::ERRORCODE_INTERNAL_ERROR) {
            return internalError("get member role");
        } else if (roleRet == dao::ERRORCODE_NO_SUCH_DATA) {
            response.code = group::ERRORCODE_NO_SUCH_DATA;
            response.msg = "not a group member";
            return errorResponse();
        } else {
            return internalError("get member role");
        }
    }

    if (role != GroupUser::ROLE_OWNER) {
        response.code = group::ERRORCODE_FORBIDDEN;
        response.msg = "not group owner";
        return errorResponse();
    }

    updateGroupBody->updateTime = nowInMilli();
    nlohmann::json upData = *updateGroupBody;
    LOGD << "update group data: " << account->uid() << ": " << gid << ": " << upData;

    auto upRet = m_groups->update(gid, upData);
    if (upRet != dao::ERRORCODE_SUCCESS) {
        return internalError("update group table");
    }

    LOGD << "update group table ok: " << account->uid() << ": " << gid;

    response.code = group::ERRORCODE_SUCCESS;
    response.msg = "success";
    context.responseEntity = response;

    Group group;
    auto queryRet = m_groups->get(gid, group);
    if (queryRet != dao::ERRORCODE_SUCCESS) {
        return internalError("get group info");
    }

    nlohmann::json msg = group;
    msg["owner"] = account->uid();
    msg.erase("permission");
    msg.erase("gid");
    msg.erase("status");
    msg.erase("encrypted_notice");

    pubGroupSystemMessage(account->uid(), gid, GROUP_INFO_UPDATE, msg.dump());

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onUpdateGroupInfo",
        (nowInMicro() - dwStartTime), group::ERRORCODE_SUCCESS);
}

void GroupManagerController::onInviteGroupMember(HttpContext& context)
{
    context.response.result(static_cast<unsigned>(bcm::custom_http_status::upgrade_requried));
    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName,
                                                         "onInviteGroupMember",
                                                         0, 
                                                         context.response.result_int());
    return;
}

void GroupManagerController::onKickGroupMember(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    res.result(http::status::ok);

    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& statics = context.statics;
    statics.setUid(account->uid());

    auto* kickGroupMemberBody = boost::any_cast<KickGroupMemberBody>(&context.requestEntity);
    uint64_t gid = kickGroupMemberBody->gid;
    GroupResponse response;

    auto errorResponse = [&]() {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onKickGroupMember",
            (nowInMicro() - dwStartTime), response.code);
        LOGW << "error, uid: " << account->uid() << " code: " << response.code << " msg:" << response.msg;
        context.responseEntity = response;
    };

    auto internalError = [&](const std::string& op) {
        LOGW << "internal error, uid: " << account->uid() << " op: " << op;
        response.code = group::ERRORCODE_INTERNAL_ERROR;
        response.msg = "internal error";
        context.responseEntity = response;
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onKickGroupMember",
            (nowInMicro() - dwStartTime), group::ERRORCODE_INTERNAL_ERROR);
    };

    bool checked = kickGroupMemberBody->check(response, account->uid());
    if (!checked) {
        return errorResponse();
    }

    GroupUser::Role role;
    auto roleRet = m_groupUsers->getMemberRole(gid, account->uid(), role);

    LOGD << "get member role: " << account->uid() << ": " << gid << ": " << roleRet << ": " << role;

    if (roleRet != dao::ERRORCODE_SUCCESS) {
        if (roleRet == dao::ERRORCODE_INTERNAL_ERROR) {
            return internalError("get member role");
        } else if (roleRet == dao::ERRORCODE_NO_SUCH_DATA) {
            response.code = group::ERRORCODE_NO_SUCH_DATA;
            response.msg = "not a group member";
            return errorResponse();
        } else {
            return internalError("get member role");
        }
    }

    if (role != GroupUser::ROLE_OWNER) {
        response.code = group::ERRORCODE_FORBIDDEN;
        response.msg = "not group owner";
        return errorResponse();
    }

    std::vector<GroupUser> users;
    auto queryRet = m_groupUsers->getMemberBatch(gid, kickGroupMemberBody->members, users);

    if (queryRet != dao::ERRORCODE_SUCCESS) {
        return internalError("get member batch");
    }

    std::map<std::string, GroupUser> memberInfo;
    std::vector<std::string> toKickMember;

    for (const auto& m: users) {
        memberInfo[m.uid()] =  m;
    }

    GroupSysMsgBody msg;
    msg.action = QUIT_GROUP;

    for(const auto& m : kickGroupMemberBody->members) {
        if (memberInfo.find(m) != memberInfo.end()) {
            toKickMember.push_back(m);
            msg.members.emplace_back(memberInfo[m].uid(), memberInfo[m].uid(), memberInfo[m].role());
        }
    }

    auto kickRet = m_groupUsers->delMemberBatch(gid, toKickMember);

    if (kickRet != dao::ERRORCODE_SUCCESS) {
        return internalError("del member batch");
    }

    pubGroupSystemMessage(account->uid(), gid, GROUP_MEMBER_UPDATE, nlohmann::json(msg).dump());

    for (const auto& m : toKickMember) {
        publishGroupUserEvent(gid, m, INTERNAL_USER_QUIT_GROUP);
    }

    response.code = group::ERRORCODE_SUCCESS;
    response.msg = "success";
    context.responseEntity = response;

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onKickGroupMember",
        (nowInMicro() - dwStartTime), group::ERRORCODE_SUCCESS);
}

void GroupManagerController::onLeaveGroup(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    res.result(http::status::ok);

    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& statics = context.statics;
    statics.setUid(account->uid());

    auto* leaveGroupBody = boost::any_cast<LeaveGroupBody>(&context.requestEntity);
    uint64_t gid = leaveGroupBody->gid;
    GroupResponse response;

    auto errorResponse = [&]() {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onLeaveGroup",
            (nowInMicro() - dwStartTime), response.code);
        LOGW << "error, uid: " << account->uid() << " code: " << response.code << " msg:" << response.msg;
        context.responseEntity = response;
    };

    auto internalError = [&](const std::string& op) {
        LOGW << "internal error, uid: " << account->uid() << " op: " << op;
        response.code = group::ERRORCODE_INTERNAL_ERROR;
        response.msg = "internal error";
        context.responseEntity = response;
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onLeaveGroup",
            (nowInMicro() - dwStartTime), group::ERRORCODE_INTERNAL_ERROR);
    };

    bool checked = leaveGroupBody->check(response);
    if (!checked) {
        return errorResponse();
    }

    auto role = GroupUser::ROLE_UNDEFINE;
    auto nextOwnerRole = GroupUser::ROLE_UNDEFINE;

    dao::GroupCounter counter;
    auto roleRet = m_groupUsers->queryGroupMemberInfoByGid(gid, counter, account->uid(), role, leaveGroupBody->nextOwner, nextOwnerRole);

    if (roleRet != dao::ERRORCODE_SUCCESS) {
        return internalError("get group role info");
    }

    if (role != GroupUser::ROLE_OWNER && role != GroupUser::ROLE_MEMBER) {
        response.code = group::ERRORCODE_FORBIDDEN;
        response.msg = "not a group member";
        return errorResponse();
    }

    if (counter.memberCnt == 0) {
        response.code = group::ERRORCODE_FORBIDDEN;
        response.msg = "empty group";
        return errorResponse();
    }

    bool needNextOwner = false;
    if (role == GroupUser::ROLE_OWNER && counter.memberCnt > 1) {
        needNextOwner = true;
    }

    if (needNextOwner) {
        const auto& nextOwner = leaveGroupBody->nextOwner;
        if (nextOwner.empty() || nextOwner == account->uid() ||
                (nextOwnerRole != GroupUser::ROLE_OWNER && nextOwnerRole != GroupUser::ROLE_MEMBER)) {
            response.code = group::ERRORCODE_PARAM_INCORRECT;
            response.msg = "invalid next owner";
            return errorResponse();
        }
    }

    if (needNextOwner) {
        auto upRet = m_groupUsers->update(gid, leaveGroupBody->nextOwner, nlohmann::json{{"role", static_cast<int32_t>(GroupUser::ROLE_OWNER)}});

        LOGD << "group user update: " << gid << ": " << leaveGroupBody->nextOwner << ": " << upRet;

        if (upRet != dao::ERRORCODE_SUCCESS) {
            return internalError("update next owner role");
        }
    }

    auto delRet = m_groupUsers->delMember(gid, account->uid());

    if (delRet != dao::ERRORCODE_SUCCESS) {
        return internalError("del old owner role");
    }

    if (needNextOwner) {
        GroupSysMsgBody upMsg;
        upMsg.action = UPDATE_INFO;
        auto nextOwner = leaveGroupBody->nextOwner;
        upMsg.members.emplace_back(nextOwner,nextOwner, GroupUser::ROLE_OWNER);
        pubGroupSystemMessage(account->uid(), gid, GROUP_MEMBER_UPDATE, nlohmann::json(upMsg).dump());

        publishGroupUserEvent(gid, nextOwner, INTERNAL_USER_CHANGE_ROLE);
    }

    GroupSysMsgBody quitMsg;
    quitMsg.action = QUIT_GROUP;
    quitMsg.members.emplace_back(account->uid(), "", GroupUser::ROLE_UNDEFINE);
    pubGroupSystemMessage(account->uid(), gid, GROUP_MEMBER_UPDATE, nlohmann::json(quitMsg).dump());

    publishGroupUserEvent(gid, account->uid(), INTERNAL_USER_QUIT_GROUP);

    response.code = group::ERRORCODE_SUCCESS;
    response.msg = "success";
    context.responseEntity = response;

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onLeaveGroup",
        (nowInMicro() - dwStartTime), group::ERRORCODE_SUCCESS);
}

void GroupManagerController::onQueryGroupInfo(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& res = context.response;
    res.result(http::status::ok);

    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& statics = context.statics;
    statics.setUid(account->uid());

    auto* queryGroupBody = boost::any_cast<QueryGroupInfoBody>(&context.requestEntity);
    GroupResponse response;

    auto errorResponse = [&]() {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onQueryGroupInfo",
            (nowInMicro() - dwStartTime), response.code);
        LOGW << "error, uid: " << account->uid() << " code: " << response.code << " msg:" << response.msg;
        context.responseEntity = response;
    };

    auto internalError = [&](const std::string& op) {
        LOGW << "internal error, uid: " << account->uid() << " op: " << op;
        response.code = group::ERRORCODE_INTERNAL_ERROR;
        response.msg = "internal error";
        context.responseEntity = response;
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onQueryGroupInfo",
            (nowInMicro() - dwStartTime), group::ERRORCODE_INTERNAL_ERROR);
    };

    bool checked = queryGroupBody->check(response);
    if (!checked) {
        return errorResponse();
    }

    dao::UserGroupDetail detail;
    dao::ErrorCode ret = dao::ERRORCODE_SUCCESS;
    ret = m_groupUsers->getGroupDetailByGid(queryGroupBody->gid, account->uid(), detail);
    LOGD << "get group detail: " << queryGroupBody->gid << ": "
         << account->uid() << ": "  << ret << ": " << nlohmann::json(detail);

    if (ret != dao::ERRORCODE_SUCCESS) {
        return internalError("get group detail");
    }

    if (detail.user.role() == GroupUser::ROLE_UNDEFINE &&
            detail.group.broadcast() == Group::BROADCAST_OFF) {
        response.code = group::ERRORCODE_FORBIDDEN;
        response.msg = "forbidden";
        return errorResponse();
    }

    response.code = group::ERRORCODE_SUCCESS;
    response.msg = "success";
    response.result = detail;
    context.responseEntity = response;

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onQueryGroupInfo",
        (nowInMicro() - dwStartTime), group::ERRORCODE_SUCCESS);
}

void GroupManagerController::onQueryGroupInfoBatch(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName,
        "onQueryGroupInfoBatch");
    context.response.result(http::status::ok);

    auto account = boost::any_cast<Account>(&context.authResult);
    context.statics.setUid(account->uid());

    auto arg = boost::any_cast<QueryGroupInfoByGidBatch>(&context.requestEntity);
    GroupResponse response;

    std::vector<dao::UserGroupEntry> entries;
    dao::ErrorCode ec = m_groupUsers->getGroupDetailByGidBatch(arg->gids,
                                                               account->uid(),
                                                               entries);
    if (dao::ERRORCODE_SUCCESS != ec) {
        LOGE << "error invoking getGroupDetailByGidBatch, uid: "
             << account->uid() << ": " << ec;
        context.responseEntity = GroupResponse(group::ERRORCODE_INTERNAL_ERROR,
                                               "database error");
        marker.setReturnCode(group::ERRORCODE_INTERNAL_ERROR);
        return;
    }

    GroupResponse resp;
    nlohmann::json groups = nlohmann::json::array();
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        if (it->user.role() == GroupUser::ROLE_UNDEFINE &&
            it->group.broadcast() == Group::BROADCAST_OFF) {
            LOGW << "user '" << account->uid()
                 << "' has no permission to query group '" << it->group.gid()
                 << "' named '" << it->group.name() << "'";
            continue;
        }
        groups.emplace_back(*it);
    }

    resp.result["groups"] = groups;
    context.responseEntity = resp;
}

void GroupManagerController::onQueryJoinGroupList(HttpContext& context) // TODO: deprecated
{
    int64_t dwStartTime = nowInMicro();
    auto& response = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    response.result(http::status::ok);

    LOGI << "receive to query group list.(uid:" << account->uid() << ")";

    //query joined group list.
    std::vector<dao::UserGroupDetail> vecGroups;
    if (m_groupUsers->getJoinedGroupsList(account->uid(), vecGroups) != dao::ERRORCODE_SUCCESS) {
        context.responseEntity = GroupResponse(group::ERRORCODE_INTERNAL_ERROR, "internal server error");
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onQueryJoinGroupList",
            (nowInMicro() - dwStartTime), group::ERRORCODE_INTERNAL_ERROR);
        LOGE << "failed to query joined group list.(uid:" << account->uid() << ")";
        return;
    }

    nlohmann::json arrayGroupList;
    for (std::vector<dao::UserGroupDetail>::iterator itVecGroup = vecGroups.begin();
        itVecGroup != vecGroups.end(); itVecGroup++) {
        nlohmann::json jsGroup;
        jsGroup["gid"] = itVecGroup->group.gid();
        jsGroup["name"] = itVecGroup->group.name();
        jsGroup["icon"] = itVecGroup->group.icon();
        jsGroup["intro"] = itVecGroup->group.intro();
        jsGroup["encrypted"] = itVecGroup->group.encryptstatus();
        jsGroup["encrypted_key"] = itVecGroup->user.encryptedkey();
        jsGroup["broadcast"] = itVecGroup->group.broadcast();
        jsGroup["channel"] = itVecGroup->group.channel();
        jsGroup["last_mid"] = itVecGroup->group.lastmid();
        jsGroup["last_ack_mid"] = itVecGroup->user.lastackmid();
        jsGroup["role"] = itVecGroup->user.role();
        jsGroup["notice"] = jsonable::safe_parse(itVecGroup->group.notice());
        jsGroup["create_time"] = itVecGroup->group.createtime();
        jsGroup["owner"] = itVecGroup->counter.owner;
        jsGroup["member_cn"] = itVecGroup->counter.memberCnt;
        jsGroup["subscriber_cn"] = itVecGroup->counter.subscriberCnt;
        jsGroup["share_qr_code_setting"] = itVecGroup->group.shareqrcodesetting();
        jsGroup["owner_confirm"] = itVecGroup->group.ownerconfirm();
        jsGroup["share_sig"] = itVecGroup->group.sharesignature();
        jsGroup["share_and_owner_confirm_sig"] = itVecGroup->group.shareandownerconfirmsignature();
        jsGroup["group_info_secret"] = itVecGroup->user.groupinfosecret();
        jsGroup["proof"] = itVecGroup->user.proof();

        arrayGroupList.push_back(jsGroup);

        LOGT << "uid: " << account->uid()
             << ", gid: " << itVecGroup->group.gid()
             << ", name: " << itVecGroup->group.name()
             << ", last_mid: " << itVecGroup->group.lastmid()
             << ", last_ack_mid " << itVecGroup->user.lastackmid();
    }

    //response group list.
    GroupResponse resBody(group::ERRORCODE_SUCCESS, "success");
    resBody.result = nlohmann::json{{"groups", arrayGroupList}};
    context.responseEntity = resBody;

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onQueryJoinGroupList",
        (nowInMicro() - dwStartTime), group::ERRORCODE_SUCCESS);

    LOGI << "success to query group join list.(" << resBody.result.dump() << ")";

}

void GroupManagerController::onQueryGroupMemberList(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& response = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    response.result(http::status::ok);

    QueryGroupMemberList* pRequest = boost::any_cast<QueryGroupMemberList>(&context.requestEntity);
    std::stringstream ss;
    ss << "[";
    for (int r : pRequest->role) {
        if (ss.tellp() > 1) {
            ss << ", ";
        }
        ss << r;
    }
    ss << "]";
    LOGI << "receive to query group member list.(uid:" << account->uid() << " gid:" << pRequest->gid << " role:" << ss.str() << ")";

    //query group member info.
    GroupUser op_user;
    if (m_groupUsers->getMember(pRequest->gid, account->uid(), op_user) != dao::ERRORCODE_SUCCESS) {
        context.responseEntity = GroupResponse(group::ERRORCODE_INTERNAL_ERROR, "internal server error");
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onQueryGroupMemberList",
            (nowInMicro() - dwStartTime), group::ERRORCODE_INTERNAL_ERROR);
        LOGE << "failed to query my group user info.(myuid:" << account->uid() << " gid:"<< pRequest->gid << ")";
        return;
    }

    if (op_user.uid() == "") {
        context.responseEntity = GroupResponse(group::ERRORCODE_NO_PERMISSION, "no permission");
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onQueryGroupMemberList",
            (nowInMicro() - dwStartTime), group::ERRORCODE_NO_PERMISSION);
        LOGE << "no permission to query group user info.(myuid:" << account->uid() << " gid:"<< pRequest->gid << ")";
        return;
    }

    //query group member list.
    std::vector<GroupUser::Role> vecRoles;
    vecRoles.reserve(pRequest->role.size());
    for (int r : pRequest->role) {
        vecRoles.push_back(GroupUser::Role(r));
    }
    std::vector<GroupUser> vecGroupUsers;
    if (m_groupUsers->getMemberRangeByRolesBatch(pRequest->gid, vecRoles, vecGroupUsers) != dao::ERRORCODE_SUCCESS) {
        context.responseEntity = GroupResponse(group::ERRORCODE_INTERNAL_ERROR, "internal server error");
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onQueryGroupMemberList",
            (nowInMicro() - dwStartTime), group::ERRORCODE_INTERNAL_ERROR);
        LOGE << "failed to read group member list.(myuid:" << account->uid() << " gid:"<< pRequest->gid << ")";
        return;
    }

    nlohmann::json memberList;
    for (std::vector<GroupUser>::iterator itVecGroupUser = vecGroupUsers.begin();
        itVecGroupUser != vecGroupUsers.end(); itVecGroupUser++) {
        nlohmann::json jsMember = nlohmann::json{{"uid", itVecGroupUser->uid()},
                                {"nick", itVecGroupUser->nick()},
                                {"nickname", itVecGroupUser->nickname()},
                                {"group_nickname", itVecGroupUser->groupnickname()},
                                {"profile_keys", itVecGroupUser->profilekeys()},
                                {"role", itVecGroupUser->role()},
                                {"encrypted_key", itVecGroupUser->encryptedkey()},
                                {"create_time", itVecGroupUser->createtime()},
                                {"proof", itVecGroupUser->proof()}
                                };
        memberList.push_back(jsMember);

    }

    //response member list.
    GroupResponse resBody(group::ERRORCODE_SUCCESS, "success");
    resBody.result = nlohmann::json{{"members", memberList}};
    context.responseEntity = resBody;

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onQueryGroupMemberList",
        (nowInMicro() - dwStartTime), group::ERRORCODE_SUCCESS);

    LOGI << "success to query group member list.(" << resBody.result.dump() << ")";

}

void GroupManagerController::onQueryGroupMemberListSegment(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName,
        "onQueryGroupMemberListSegment");

    QueryGroupMemberListSeg* arg = boost::any_cast<QueryGroupMemberListSeg>(
        &context.requestEntity);
    Account* account = boost::any_cast<Account>(&context.authResult);
    context.response.result(http::status::ok);

    std::stringstream ss;
    ss << "[";
    for (int r : arg->role) {
        if (ss.tellp() > 1) {
            ss << ", ";
        }
        ss << r;
    }
    ss << "]";

    LOGI << "receive to query group member list.(uid:" << account->uid()
         << " gid:" << arg->gid << " role:" << ss.str() << ")";

    GroupUser op_user;
    dao::ErrorCode ec = m_groupUsers->getMember(arg->gid, account->uid(),
        op_user);
    if (dao::ERRORCODE_SUCCESS != ec) {
        context.responseEntity = GroupResponse(group::ERRORCODE_INTERNAL_ERROR,
                                               "internal server error");
        marker.setReturnCode(group::ERRORCODE_INTERNAL_ERROR);
        LOGE << "failed to query my group user info.(myuid:" << account->uid()
             << " gid:"<< arg->gid << "): " << ec;
        return;
    }
    if (op_user.uid() == "") {
        context.responseEntity = GroupResponse(group::ERRORCODE_NO_PERMISSION,
                                               "no permission");
        marker.setReturnCode(group::ERRORCODE_NO_PERMISSION);
        LOGE << "no permission to query group user info.(myuid:"
             << account->uid() << " gid:"<< arg->gid << ")";
        return;
    }

    std::vector<GroupUser::Role> roles;
    roles.reserve(arg->role.size());
    for (int r : arg->role) {
        roles.push_back(GroupUser::Role(r));
    }
    std::vector<GroupUser> members;
    ec = m_groupUsers->getMemberRangeByRolesBatchWithOffset(arg->gid, roles,
        arg->startUid, arg->count, members);
    if (dao::ERRORCODE_NO_SUCH_DATA == ec) {
        // don't treat this as an error
    } else if (dao::ERRORCODE_SUCCESS != ec) {
        context.responseEntity = GroupResponse(group::ERRORCODE_INTERNAL_ERROR,
                                               "internal server error");
        marker.setReturnCode(group::ERRORCODE_INTERNAL_ERROR);
        LOGE << "failed to read group member list.(myuid:" << account->uid()
             << " gid:"<< arg->gid << "): " << ec;
        return;
    }

    nlohmann::json memberList = nlohmann::json::array();
    for (auto it = members.begin(); it != members.end(); ++it) {
        nlohmann::json m = nlohmann::json{
            {"uid", it->uid()},
            {"nick", it->nick()},
            {"nickname", it->nickname()},
            {"group_nickname", it->groupnickname()},
            {"profile_keys", it->profilekeys()},
            {"role", it->role()},
            {"encrypted_key", it->encryptedkey()},
            {"create_time", it->createtime()},
            {"proof", it->proof()}
        };
        memberList.push_back(m);
    }

    GroupResponse resBody(group::ERRORCODE_SUCCESS, "success");
    resBody.result = nlohmann::json{{"members", memberList}};
    context.responseEntity = resBody;

    LOGT << "success to query group member list.(" << resBody.result.dump() << ")";
}

void GroupManagerController::onQueryGroupMemberInfo(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& response = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    response.result(http::status::ok);

    QueryGroupMemberInfo* pRequest = boost::any_cast<QueryGroupMemberInfo>(&context.requestEntity);
    LOGI << "receive to query group member info.(uid:" << account->uid() << " gid:" << pRequest->gid << " query-uid:" << pRequest->uid << ")";

    //check operator.
    GroupUser op_user;
    if (m_groupUsers->getMember(pRequest->gid, account->uid(), op_user) != dao::ERRORCODE_SUCCESS) {
        context.responseEntity = GroupResponse(group::ERRORCODE_INTERNAL_ERROR, "internal server error");
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onQueryGroupMemberInfo",
            (nowInMicro() - dwStartTime), group::ERRORCODE_INTERNAL_ERROR);
        LOGE << "failed to query my group user info.(myuid:" << account->uid() << " gid:"<< pRequest->gid << ")";
        return;
    }

    if (op_user.uid() == "") {
        context.responseEntity = GroupResponse(group::ERRORCODE_NO_PERMISSION, "no permission");
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onQueryGroupMemberInfo",
            (nowInMicro() - dwStartTime), group::ERRORCODE_NO_PERMISSION);
        LOGE << "no permission to query group user info.(myuid:" << account->uid() << " gid:"<< pRequest->gid << ")";
        return;
    }

    //query user.
    GroupUser user;
    if (m_groupUsers->getMember(pRequest->gid, pRequest->uid, user) != dao::ERRORCODE_SUCCESS) {
        context.responseEntity = GroupResponse(group::ERRORCODE_INTERNAL_ERROR, "internal server error");
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onQueryGroupMemberInfo",
            (nowInMicro() - dwStartTime), group::ERRORCODE_INTERNAL_ERROR);
        LOGE << "failed to query group user info.(gid:" << pRequest->gid << " uid:" << pRequest->uid << ")";
        return;
    }

    if (op_user.uid() == "") {
        context.responseEntity = GroupResponse(group::ERRORCODE_NOT_FIND_USER, "not find user");
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onQueryGroupMemberInfo",
            (nowInMicro() - dwStartTime), group::ERRORCODE_NOT_FIND_USER);
        LOGE << "not find group user.(gid:" << pRequest->gid << " uid:" << pRequest->uid << ")";
        return;
    }

    //response member list.
    GroupResponse resBody(group::ERRORCODE_SUCCESS, "success");
    resBody.result = nlohmann::json{
                            {"uid", pRequest->uid},
                            {"nick", user.nick()},
                            {"role", user.role()},
                            {"status", user.status()},
                            {"nickname", user.nickname()},
                            {"group_nickname", user.groupnickname()},
                            {"profile_keys", user.profilekeys()},
                            {"encrypted_key", user.encryptedkey()},
                            {"group_info_secret", user.groupinfosecret()},
                            {"create_time", user.createtime()},
                            {"proof", user.proof()}
                            };
    context.responseEntity = resBody;
    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onQueryGroupMemberInfo",
        (nowInMicro() - dwStartTime), group::ERRORCODE_SUCCESS);

    LOGI << "success to query group member info.(" << resBody.result.dump() << ")";

}

void GroupManagerController::onUpdateGroupUserInfo(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& response = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    response.result(http::status::ok);

    UpdateGroupUserBody* pRequest = boost::any_cast<UpdateGroupUserBody>(&context.requestEntity);
    LOGI << "receive to update group user body.(uid:" << account->uid() << " gid:"<< pRequest->gid << ")";

    if ((pRequest->mute == 0xff) && (pRequest->nick == "") &&
            (pRequest->nickname.empty()) && (pRequest->groupNickname.empty()) &&
            (pRequest->profileKeys.empty())) {
        context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, "success");
        LOGW << "group user info no update.(uid:" << account->uid() << " gid:"<< pRequest->gid << ")";
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onUpdateGroupUserInfo",
            (nowInMicro() - dwStartTime), static_cast<int>(group::ERRORCODE_SUCCESS));
        return;
    }

    GroupUser user;
    if (m_groupUsers->getMember(pRequest->gid, account->uid(), user) != dao::ERRORCODE_SUCCESS) {
        context.responseEntity = GroupResponse(group::ERRORCODE_INTERNAL_ERROR, "internal server error");
        LOGE << "failed to query group user info.(uid:" << account->uid() << " gid:"<< pRequest->gid << ")";
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onUpdateGroupUserInfo",
            (nowInMicro() - dwStartTime), static_cast<int>(group::ERRORCODE_INTERNAL_ERROR));
        return;
    }

    if (user.uid() == "") {
        context.responseEntity = GroupResponse(group::ERRORCODE_NO_PERMISSION, "no permission");
        LOGE << "no permission to update group user info.(uid:" << account->uid() << " gid:"<< pRequest->gid << ")";
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onUpdateGroupUserInfo",
            (nowInMicro() - dwStartTime), static_cast<int>(group::ERRORCODE_NO_PERMISSION));
        return;
    }

    //update group user info.
    nlohmann::json upData;
    if (pRequest->mute != 0xff) {
        int nstatus = user.status();
        if (pRequest->mute == 1) {
            nstatus = nstatus | 1;
        } else {
            nstatus = nstatus & ~1;
        }

        upData["status"] = nstatus;
    }

    if (!pRequest->nick.empty()) {
        upData["nick"] = pRequest->nick;
    }

    if (!pRequest->nickname.empty()) {
        upData["nick_name"] = pRequest->nickname;
    }

    if (!pRequest->groupNickname.empty()) {
        upData["group_nick_name"] = pRequest->groupNickname;
    }

    if (!pRequest->profileKeys.empty()) {
        upData["profile_keys"] = pRequest->profileKeys;
    }

    if (m_groupUsers->update(pRequest->gid, account->uid(), upData) != dao::ERRORCODE_SUCCESS) {
        context.responseEntity = GroupResponse(group::ERRORCODE_INTERNAL_ERROR, "internal server error");
        LOGE << "failed to save group user info.(uid:" << account->uid() << " gid:"<< pRequest->gid << ")";
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onUpdateGroupUserInfo",
            (nowInMicro() - dwStartTime), static_cast<int>(group::ERRORCODE_INTERNAL_ERROR));
        return;
    }

    //notify group message.
    if (pRequest->mute != 0xff) {
        InternalMessageType type = (pRequest->mute == 1) ? INTERNAL_USER_MUTE_GROUP : INTERNAL_USER_UNMUTE_GROUP;
        publishGroupUserEvent(pRequest->gid, account->uid(), type);
    }

    context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, "success");
    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onUpdateGroupUserInfo",
        (nowInMicro() - dwStartTime), group::ERRORCODE_SUCCESS);

    LOGI << "success to update group user body.(uid:" << account->uid() << " gid:"<< pRequest->gid << ")";
}

void GroupManagerController::onUpdateGroupNotice(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& response = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    response.result(http::status::ok);

    UpdateGroupBody* pRequest = boost::any_cast<UpdateGroupBody>(&context.requestEntity);
    LOGI << "receive to update group notice.(uid:" << account->uid() << " gid:"<< pRequest->gid << ")";

    GroupUser::Role role;
    if ((m_groupUsers->getMemberRole(pRequest->gid, account->uid(), role) != dao::ERRORCODE_SUCCESS) || (role == GroupUser::ROLE_UNDEFINE)) {
        context.responseEntity = GroupResponse(group::ERRORCODE_INTERNAL_ERROR, "internal server error");
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onUpdateGroupNotice",
            (nowInMicro() - dwStartTime), static_cast<int>(group::ERRORCODE_INTERNAL_ERROR));
        LOGE << "failed to group member role.(uid:" << account->uid() << " gid:"<< pRequest->gid << ")";
        return;
    }

    if (role != static_cast<int>(GroupUser::ROLE_OWNER)) {
        context.responseEntity = GroupResponse(group::ERRORCODE_INTERNAL_ERROR, "internal server error");
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onUpdateGroupNotice",
            (nowInMicro() - dwStartTime), static_cast<int>(group::ERRORCODE_INTERNAL_ERROR));
        LOGE << "no permission to update group notice.(uid:" << account->uid() << " gid:"<< pRequest->gid << ")";
        return;
    }

    //update group notice.
    nlohmann::json noticeData = nlohmann::json{{"content", pRequest->notice}, {"updateTime", nowInMilli()}};
    nlohmann::json upData = nlohmann::json{{"update_time", nowInMilli()}, {"notice", noticeData}};
    if (m_groups->update(pRequest->gid, upData) != dao::ERRORCODE_SUCCESS) {
        context.responseEntity = GroupResponse(group::ERRORCODE_INTERNAL_ERROR, "internal server error");
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onUpdateGroupNotice",
            (nowInMicro() - dwStartTime), static_cast<int>(group::ERRORCODE_INTERNAL_ERROR));
        LOGE << "failed to update group notice.(uid:" << account->uid() << " gid:"<< pRequest->gid << ")";
        return;
    }

    //query group info.
    Group group;
    if (m_groups->get(pRequest->gid, group) != dao::ERRORCODE_SUCCESS) {//TODO:justinfang - if not find ???
        context.responseEntity = GroupResponse(group::ERRORCODE_INTERNAL_ERROR, "internal server error");
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onUpdateGroupNotice",
            (nowInMicro() - dwStartTime), static_cast<int>(group::ERRORCODE_INTERNAL_ERROR));
        LOGE << "failed to read group info.(uid:" << account->uid() << " gid:"<< pRequest->gid << ")";
        return;
    }

    nlohmann::json message = nlohmann::json{
                                    {"owner", account->uid()},
                                    {"name", group.name()},
                                    {"icon", group.icon()},
                                    {"update_time", group.updatetime()},
                                    {"create_time", group.createtime()},
                                    {"last_mid", group.lastmid()},
                                    {"broadcast", group.broadcast()},
                                    {"channel", group.channel()},
                                    {"intro", group.intro()},
                                    {"key", group.key()},
                                    {"encrypted", group.encryptstatus()},
                                    {"plain_channel_key", group.plainchannelkey()},
                                    {"notice", group.notice()}
                                    };

    //publish group info update message.
    if (!pubGroupSystemMessage(account->uid(), pRequest->gid, GROUP_INFO_UPDATE, message.dump())) {//TODO:justinfang - if not find ???
        context.responseEntity = GroupResponse(group::ERRORCODE_INTERNAL_ERROR, "internal server error");
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onUpdateGroupNotice",
            (nowInMicro() - dwStartTime), static_cast<int>(group::ERRORCODE_INTERNAL_ERROR));
        LOGE << "failed to write group message.(uid:" << account->uid() << " gid:"<< pRequest->gid << ")";
        return;
    }

    //response update result.
    context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, "success");
    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "onUpdateGroupNotice",
        (nowInMicro() - dwStartTime), group::ERRORCODE_SUCCESS);
    
    LOGI << "success to finish update group notice.(uid:" << account->uid() << " gid:"<< pRequest->gid << ")";
}

bool GroupManagerController::pubGroupSystemMessage(const std::string& uid, const uint64_t groupid, const int type, const std::string& strText)
{
    uint64_t mid;
    return pubGroupSystemMessage(uid, groupid, type, strText, mid);
}

bool GroupManagerController::publishGroupUserEvent(const uint64_t& gid, const std::string& uid, const InternalMessageType& type)
{
    GroupEvent groupSystemEvent;
    groupSystemEvent.gid = gid;
    groupSystemEvent.uid = uid;
    groupSystemEvent.type = static_cast<int>(type);

    nlohmann::json jsMessage;
    to_json(jsMessage, groupSystemEvent);

    std::string strChannel = "user_" + uid;
    if (!RedisClientSync::Instance()->publish(strChannel, jsMessage.dump()))
        LOGE << "failed to publish group user event to redis.(channel:" << strChannel << ")";
        return false;

    LOGI << "success to publish group user event to redis.(channel:" << strChannel << ")";
    return true;
}

bool GroupManagerController::getGroupOwner(uint64_t gid, bcm::Account& owner)
{
    std::string ownerUid;
    dao::ErrorCode ec = m_groupUsers->getGroupOwner(gid, ownerUid);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "group owner data corrupted, gid: " << gid << ", error: " << ec;
        return false;
    }
    ec = m_accountsManager->get(ownerUid, owner);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to get account, uid: " << ownerUid << ", error: " << ec;
        return false;
    }
    return true;
}

bool GroupManagerController::verifyGroupSetting(const std::string& qrCodeSetting,
                                                const std::string& shareSignature,
                                                int ownerConfirm,
                                                const std::string& shareAndOwnerConfrimSignature,
                                                const std::string& ownerPublicKey)
{
    std::string decoded("");
    // if the settings of this group is uninitialized, ignore the validation
    if (!shareSignature.empty()) {
        decoded = Base64::decode(qrCodeSetting);
        if (!AccountHelper::verifySignature(ownerPublicKey, decoded, shareSignature)) {
            LOGE << "share setting verify failed, setting: " << qrCodeSetting
                 << ", signature: " << shareSignature;
            return false;
        }
    }
    if (!shareAndOwnerConfrimSignature.empty()) {
        std::ostringstream oss;
        oss << decoded << ownerConfirm;
        if (!AccountHelper::verifySignature(ownerPublicKey, oss.str(), shareAndOwnerConfrimSignature)) {
            LOGE << "share setting verify failed, setting: " << qrCodeSetting
                 << ", ownerConfirm: " << ownerConfirm << ", signature: " << shareAndOwnerConfrimSignature;
            return false;
        }
    }
    return true;
}

void GroupManagerController::onGroupJoinRequst(HttpContext& context)
{
    LOGT << "onGroupJoinRequst start...";
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onGroupJoinRequst");

    auto& response = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);

    GroupJoinRequest* req = boost::any_cast<GroupJoinRequest>(&context.requestEntity);
    std::string error;
    if (!req->check(error)) {
        LOGE << error;
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }
    bcm::Group group;
    dao::ErrorCode ec = m_groups->get(req->gid, group);
    if (ec == dao::ERRORCODE_NO_SUCH_DATA) {
        LOGE << "group not existed, gid: " << req->gid << ", error: " << ec;
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to get group, gid: " << req->gid << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }
    if (group.version() == static_cast<int32_t>(group::GroupVersion::GroupV3)) {
        LOGE << "can not join v3 group, gid: " << req->gid << ", uid: " << account->uid();
        response.result(static_cast<unsigned>(bcm::custom_http_status::upgrade_requried));
        marker.setReturnCode(response.result_int());
        return;
    }

    bcm::Account owner;
    if (!getGroupOwner(group.gid(), owner)) {
        LOGE << "failed to get owner, gid: " << group.gid();
        marker.setReturnCode(group::ERRORCODE_INTERNAL_ERROR);
        response.result(http::status::internal_server_error);
        return;
    }
    // check the data consistency to make sure the settings in database are correct
    if (group.shareqrcodesetting().empty()
            || group.sharesignature().empty()
            || group.shareandownerconfirmsignature().empty()) {
        LOGE << "group setting verify failed, gid: " << req->gid << ", owner confirm: " << group.ownerconfirm()
             << ", setting: " << group.shareqrcodesetting() << ", signature: " << group.shareandownerconfirmsignature();
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }
    if (!verifyGroupSetting(group.shareqrcodesetting(),
                            group.sharesignature(),
                            group.ownerconfirm(),
                            group.shareandownerconfirmsignature(),
                            owner.publickey())) {
        LOGE << "group setting verify failed, gid: " << req->gid << ", owner confirm: " << group.ownerconfirm()
             << ", setting: " << group.shareqrcodesetting() << ", signature: " << group.shareandownerconfirmsignature();
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }
    // check the qr code is correct
    if (req->qrCodeToken != group.sharesignature()) {
        LOGE << "invalid qr code, gid:" << group.gid() << ", qr code token: " << req->qrCodeToken
             << ", share signature: " << group.sharesignature();
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }
    std::ostringstream oss;
    oss << Base64::decode(req->qrCode) << Base64::decode(req->qrCodeToken);
    // check the signature of this user to ensure this request is actually sent by him
    if (!AccountHelper::verifySignature(account->publickey(), oss.str(), req->signature)) {
        LOGE << "invalid signature, gid: " << req->gid << ", code: " << req->qrCode
             << ", qr code token: " << req->qrCodeToken << ", signature: " << req->signature;
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }

    bcm::GroupUser user;
    ec = m_groupUsers->getMember(req->gid, account->uid(), user);
    if (ec == dao::ERRORCODE_SUCCESS) {
        LOGI << "user already existed" << req->gid << ", uid: " << account->uid() << ", error: " << ec;
        context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, "success");
        response.result(http::status::ok);
        marker.setReturnCode(response.result_int());
        return;
    }

    std::vector<SimpleGroupMemberInfo> infos;
    if (!group.ownerconfirm()) {
        // add the user into group to wait for another one to share him the group secret
        GroupUser gu;
        gu.set_gid(req->gid);
        gu.set_uid(account->uid());
        gu.set_role(GroupUser::ROLE_MEMBER);
        gu.set_nick(account->uid());
        gu.set_lastackmid(group.lastmid());
        gu.set_status(GroupUser::STATUS_DEFAULT);
        gu.set_createtime(nowInMilli());
        gu.set_updatetime(gu.createtime());
        gu.set_encryptedkey("");
        gu.set_groupinfosecret("");
        ec = m_groupUsers->insert(gu);
        if (ec != dao::ERRORCODE_SUCCESS) {
            LOGE << "failed to insert into group user, gid" << gu.gid() << ", uid: " << gu.uid() << ", error: " << ec;
            response.result(http::status::internal_server_error);
            marker.setReturnCode(response.result_int());
            return;
        }

        publishGroupUserEvent(gu.gid(), gu.uid(), INTERNAL_USER_ENTER_GROUP);

        context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, "success");
        response.result(http::status::ok);
        marker.setReturnCode(response.result_int());

        infos.emplace_back(gu.uid(), gu.uid(), gu.role());
    }
    // add this request into pending group user list to wait for owner's review
    PendingGroupUser pgu;
    pgu.set_gid(req->gid);
    pgu.set_uid(account->uid());
    pgu.set_inviter("");
    pgu.set_signature(req->signature);
    pgu.set_comment(req->comment);
    ec = m_pendingGroupUsers->set(pgu);
    if (dao::ERRORCODE_SUCCESS != ec) {
        LOGE << "failed to insert into pending group user list, gid" << pgu.gid() << ", uid: " << pgu.uid()
             << ", signature: " << pgu.signature() << ", comment: " << pgu.comment() << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }
    context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, "success");
    response.result(http::status::ok);
    marker.setReturnCode(response.result_int());

    if (!!group.ownerconfirm()) {
        // pub the message to owner
        pubReviewJoinRequestToOwner(req->gid, owner.uid());
    } else {
        // pub member update msg to issue key distribution
        GroupSysMsgBody msg{ENTER_GROUP, infos};
        pubGroupSystemMessage("", req->gid, GROUP_MEMBER_UPDATE, nlohmann::json(msg).dump());
    }
}

void GroupManagerController::onQueryGroupPendingList(HttpContext& context)
{
    LOGT << "onQueryGroupPendingList start...";
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName,
                                       "onQueryGroupPendingList");

    auto& response = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);

    GroupResponse resp;
    nlohmann::json result = nlohmann::json::array();

    if (m_multiDeviceConfig.needUpgrade) {
        auto authDevice = *AccountsManager::getAuthDevice(*account);
        auto& clientVer = authDevice.clientversion();
        std::string feature_string = AccountsManager::getFeatures(*account, authDevice.id());
        BcmFeatures features(HexEncoder::decode(feature_string));
        if (clientVer.ostype() == ClientVersion::OSTYPE_IOS  || clientVer.ostype() == ClientVersion::OSTYPE_ANDROID) {
            if(!features.hasFeature(bcm::Feature::FEATURE_MULTI_DEVICE)) {
                LOGW << "onQueryGroupPendingList: "<< "dont support multi device: " << account->uid();
                resp.result["list"] = result;
                resp.code = group::ErrorCode::ERRORCODE_SUCCESS;
                resp.msg = "success";
                context.responseEntity = resp;
                response.result(http::status::ok);
                marker.setReturnCode(response.result_int());
                return;
            }
        }
    }


    QueryGroupPendingListRequest* req = boost::any_cast<QueryGroupPendingListRequest>(&context.requestEntity);
    std::string error;
    if (!req->check(error)) {
        LOGE << error;
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }
    bcm::Group group;
    dao::ErrorCode ec = m_groups->get(req->gid, group);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to get group, gid: " << req->gid << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }

    bcm::GroupUser::Role role;
    ec = m_groupUsers->getMemberRole(req->gid, account->uid(), role);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to get role, gid: " << req->gid << ", uid: " << account->uid() << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }

    std::string ownerPublicKey;

    if (group.ownerconfirm()) {
        // check whether the caller is group owner
        if (role != GroupUser::Role::GroupUser_Role_ROLE_OWNER) {
            LOGE << "bad request, gid: " << req->gid << ", uid: " << account->uid();
            response.result(http::status::bad_request);
            marker.setReturnCode(response.result_int());
            return;
        }
        ownerPublicKey = account->publickey();
    } else {
        // do not need all group members to handle key distribution, just let the candidate do it for performance reason
        uint seed = std::chrono::duration_cast<std::chrono::hours>(
                    std::chrono::steady_clock().now().time_since_epoch()).count();
        OnlineMsgMemberMgr::UserSet candidates;
        selectKeyDistributionCandidates(req->gid, seed, m_groupConfig.keySwitchCandidateCount, candidates);
        DispatchAddress user{account->uid(), account->authdeviceid()};
        if (candidates.find(user) == candidates.end()) {
            LOGI << "ignore pending group user query, gid: " << req->gid << ", uid: " << user;
            resp.result["list"] = result;
            resp.code = group::ErrorCode::ERRORCODE_SUCCESS;
            resp.msg = "success";
            context.responseEntity = resp;
            response.result(http::status::ok);
            marker.setReturnCode(response.result_int());
            return;
        }

        bcm::Account owner;
        if (!getGroupOwner(group.gid(), owner)) {
            LOGE << "failed to get owner, gid: " << group.gid();
            response.result(http::status::internal_server_error);
            marker.setReturnCode(response.result_int());
            return;
        }
        ownerPublicKey = owner.publickey();
    }

    // check the data consistency to make sure the settings in database are correct
    if (!verifyGroupSetting(group.shareqrcodesetting(),
                            group.sharesignature(),
                            group.ownerconfirm(),
                            group.shareandownerconfirmsignature(),
                            ownerPublicKey)) {
        LOGE << "group setting verify failed, gid: " << req->gid << ", owner confirm: " << group.ownerconfirm()
             << ", setting: " << group.shareqrcodesetting() << ", signature: " << group.shareandownerconfirmsignature();
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }

    // laod database to get the pending list
    std::vector<PendingGroupUser> users;
    ec = m_pendingGroupUsers->query(req->gid, req->startUid, req->count, users);
    if (ec != dao::ERRORCODE_SUCCESS && ec != dao::ERRORCODE_NO_SUCH_DATA) {
        LOGE << "failed to query pending group users, gid: " << req->gid << ", startUid: " << req->startUid
             << ", count: " << req->count << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }

    for (const auto& u : users) {
        result.emplace_back(u);
    }

    resp.msg = "success";
    resp.code = group::ErrorCode::ERRORCODE_SUCCESS;
    resp.result["list"] = result;
    context.responseEntity = resp;
    response.result(http::status::ok);
    marker.setReturnCode(response.result_int());
}

void GroupManagerController::distributeKeys(HttpContext& context,
                                            const ReviewJoinResultList* req,
                                            const bcm::Group& group)
{
    auto& response = context.response;
    dao::ErrorCode ec;

    // load the user role if the user is already in this group
    std::set<std::string> delList;
    std::vector<std::string> uids;
    std::vector<GroupUser> users;
    std::vector<ReviewJoinResult> acceptedList;
    for (const auto& item : req->list) {
        if (!item.accepted) {
            delList.emplace(item.uid);
            continue;
        }
        uids.emplace_back(item.uid);
        acceptedList.emplace_back(item);
    }
    if (!uids.empty()) {
        ec = m_groupUsers->getMemberBatch(req->gid, uids, users);
        if (ec != dao::ERRORCODE_SUCCESS && ec != dao::ERRORCODE_NO_SUCH_DATA) {
            LOGE << "failed to get group members: " << req->gid;
            response.result(http::status::internal_server_error);
            return;
        }
    }
    std::map<std::string, GroupUser> existedUsers;
    for (const auto& u : users) {
        existedUsers.emplace(u.uid(), u);
    }

    std::vector<bcm::GroupUser> insertList;
    int64_t nowMs = nowInMilli();
    bool encrypted = (group.encryptstatus() == Group::ENCRYPT_STATUS_ON);
    std::map<std::string, std::set<std::string>> invitees;
    for (const auto& item : acceptedList) {
        auto it = existedUsers.find(item.uid);
        // check whether the user is already in this group, if not then add it into group
        if (it == existedUsers.end()) {
            // batch insert for performance reason
            GroupUser user;
            user.set_uid(item.uid);
            user.set_nick(item.uid);
            user.set_role(GroupUser::ROLE_MEMBER);
            user.set_gid(req->gid);
            user.set_lastackmid(group.lastmid());
            user.set_status(GroupUser::STATUS_DEFAULT);
            user.set_updatetime(nowMs);
            user.set_createtime(nowMs);
            if (encrypted) {
                user.set_encryptedkey(item.groupSecret);
            }
            user.set_groupinfosecret(item.groupInfoSecret);
            insertList.emplace_back(std::move(user));
            invitees[item.inviter].emplace(item.uid);
            continue;
        }
        // if the user is in group but lack of the 2 keys, update the 2 keys
        if (it->second.encryptedkey().empty() || (encrypted && it->second.groupinfosecret().empty())) {
            nlohmann::json data;
            if (it->second.encryptedkey().empty()) {
                data["encrypted_key"] = item.groupSecret;
            }
            if (encrypted && it->second.groupinfosecret().empty()) {
                data["group_info_secret"] = item.groupInfoSecret;
            }
            data["update_time"] = nowInMilli();
            ec = m_groupUsers->updateIfEmpty(req->gid, item.uid, data);
            if (dao::ERRORCODE_SUCCESS != ec && dao::ERRORCODE_ALREADY_EXSITED != ec) {
                LOGE << "failed to update group user list, uid: " << item.uid << ", data: " << data.dump()
                     << ", error: " << ec;
                response.result(http::status::internal_server_error);
                return;
            }
            if (dao::ERRORCODE_SUCCESS == ec) {
                // pub a message to target user to refresh group secret
                pubSecretRefreshToUser(req->gid, item.uid, item.groupSecret, item.groupInfoSecret);
            }
        } else {
            LOGD << item.uid << " already in group " << req->gid;
        }
        delList.emplace(item.uid);
    }

    // insert the accepted users into this group
    if (!insertList.empty()) {
        ec = m_groupUsers->insertBatch(insertList);
        if (dao::ERRORCODE_SUCCESS != ec) {
            LOGE << "failed to insert into group user, gid: " << req->gid << ", uid: ";
            std::string sep("");
            for (const auto& item : insertList) {
                LOGE << sep << item.uid();
                sep = ",";
            }
            LOGE << ", error: " << ec;
            response.result(http::status::internal_server_error);
            return;
        }
        for (const auto& it : invitees) {
            std::vector<SimpleGroupMemberInfo> members;
            for (const auto& uid : it.second) {
                publishGroupUserEvent(req->gid, uid, INTERNAL_USER_ENTER_GROUP);
                delList.emplace(uid);
                members.emplace_back(uid, uid, GroupUser::Role::GroupUser_Role_ROLE_MEMBER);
            }
            GroupSysMsgBody msg{ENTER_GROUP, members};
            //get the inviter to pub the message
            pubGroupSystemMessage(it.first, req->gid, GROUP_MEMBER_UPDATE, nlohmann::json(msg).dump());
        }
    }

    // delete those users from the pending list because the requests are complete
    if (!delList.empty()) {
        ec = m_pendingGroupUsers->del(req->gid, delList);
        if (dao::ERRORCODE_SUCCESS != ec) {
            LOGE << "failed to delete pending group user list, gid: " << req->gid << ", uid: ";
            std::string sep("");
            for (const auto& u : delList) {
                LOGE << sep << u;
                sep = ",";
            }
            response.result(http::status::internal_server_error);
            return;
        }
    }
    response.result(http::status::ok);
}

void GroupManagerController::onReviewJoinRequest(bcm::HttpContext& context)
{
    LOGT << "onReviewJoinRequest start...";
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onReviewJoinRequest");

    auto& response = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    ReviewJoinResultList* req = boost::any_cast<ReviewJoinResultList>(&context.requestEntity);
    std::string error;
    if (!req->check(error)) {
        LOGE << error;
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }

    std::string ownerUid;
    dao::ErrorCode ec = m_groupUsers->getGroupOwner(req->gid, ownerUid);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "group owner data corrupted, gid: " << req->gid << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }
    if (ownerUid != account->uid()) {
        LOGE << "bad request, gid: " << req->gid << ", uid: " << account->uid();
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }

    bcm::Group group;
    ec = m_groups->get(req->gid, group);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to get group, gid: " << req->gid << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }
    // check the data consistency to make sure the settings in database are correct
    if (!verifyGroupSetting(group.shareqrcodesetting(),
                            group.sharesignature(),
                            group.ownerconfirm(),
                            group.shareandownerconfirmsignature(),
                            account->publickey())) {
        LOGE << "group setting verify failed, gid: " << req->gid << ", owner confirm: " << group.ownerconfirm()
             << ", setting: " << group.shareqrcodesetting() << ", signature: " << group.shareandownerconfirmsignature();
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }

    distributeKeys(context, req, group);

    marker.setReturnCode(response.result_int());
    if (response.result() != http::status::ok) {
        return;
    }

    GroupResponse gresp;
    gresp.code = group::ErrorCode::ERRORCODE_SUCCESS;
    gresp.msg = "success";
    context.responseEntity = gresp;
}

void GroupManagerController::onUploadPassword(HttpContext& context)
{
    LOGT << "onUploadPassword start...";
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onUploadPassword");

    auto& response = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    ReviewJoinResultList* req = boost::any_cast<ReviewJoinResultList>(&context.requestEntity);
    std::string error;
    if (!req->check(error)) {
        LOGE << error;
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }

    bcm::GroupUser::Role role;
    dao::ErrorCode ec = m_groupUsers->getMemberRole(req->gid, account->uid(), role);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to get role, gid: " << req->gid << ", uid: " << account->uid() << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }
    // only owner, administrator and member can do this
    if (role != bcm::GroupUser::Role::GroupUser_Role_ROLE_OWNER
            && role != bcm::GroupUser::Role::GroupUser_Role_ROLE_ADMINISTROR
            && role != bcm::GroupUser::Role::GroupUser_Role_ROLE_MEMBER) {
        LOGE << "bad request, gid: " << req->gid << ", uid: " << account->uid() << ", role: " << role;
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }

    bcm::Group group;
    ec = m_groups->get(req->gid, group);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to get group, gid: " << req->gid << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }
    bcm::Account owner;
    if (!getGroupOwner(req->gid, owner)) {
        LOGE << "failed to get owner, gid: " << req->gid;
        marker.setReturnCode(group::ERRORCODE_INTERNAL_ERROR);
        response.result(http::status::internal_server_error);
        return;
    }
    if (group.shareqrcodesetting().empty()
            || group.sharesignature().empty()
            || group.shareandownerconfirmsignature().empty()) {
        LOGE << "group setting verify failed, gid: " << req->gid << ", owner confirm: " << group.ownerconfirm()
             << ", setting: " << group.shareqrcodesetting() << ", signature: " << group.shareandownerconfirmsignature();
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }
    // check the data consistency to make sure the settings in database are correct
    if (!verifyGroupSetting(group.shareqrcodesetting(),
                            group.sharesignature(),
                            group.ownerconfirm(),
                            group.shareandownerconfirmsignature(),
                            owner.publickey())) {
        LOGE << "group setting verify failed, gid: " << req->gid << ", owner confirm: " << group.ownerconfirm()
             << ", setting: " << group.shareqrcodesetting() << ", signature: " << group.shareandownerconfirmsignature();
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }

    distributeKeys(context, req, group);

    marker.setReturnCode(response.result_int());
    if (response.result() != http::status::ok) {
        return;
    }

    GroupResponse gresp;
    gresp.code = group::ErrorCode::ERRORCODE_SUCCESS;
    gresp.msg = "success";
    context.responseEntity = gresp;
}

void GroupManagerController::onCreateGroupV2(HttpContext &context)
{
    LOGT << "onCreateGroupV2 start...";
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onCreateGroupV2");

    auto& res = context.response;
    res.result(http::status::ok);

    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& statics = context.statics;
    statics.setUid(account->uid());

    if (m_multiDeviceConfig.needUpgrade) {
        auto authDevice = *AccountsManager::getAuthDevice(*account);
        auto& clientVer = authDevice.clientversion();
        std::string feature_string = AccountsManager::getFeatures(*account, authDevice.id());
        BcmFeatures features(HexEncoder::decode(feature_string));
        if (clientVer.ostype() == ClientVersion::OSTYPE_IOS  || clientVer.ostype() == ClientVersion::OSTYPE_ANDROID) {
            if(!features.hasFeature(bcm::Feature::FEATURE_MULTI_DEVICE)) {
                res.result(static_cast<unsigned>(custom_http_status::upgrade_requried));
                return;
            }
        }
    }

    if (LimitLevel::LIMITED == m_groupCreationLimiter->acquireAccess(account->uid())) {
        LOGE << "the request has been rejected";
        res.result(static_cast<unsigned>(bcm::custom_http_status::limiter_rejected));
        marker.setReturnCode(res.result_int());
        return;
    }

    GroupResponse response;

    auto* createGroupBody = boost::any_cast<CreateGroupBodyV2>(&context.requestEntity);
    bool checked = createGroupBody->check(response);

    if (!checked) {
        LOGE << "parameter error, code: " << response.code << ", " << response.msg;
        res.result(http::status::bad_request);
        marker.setReturnCode(res.result_int());
        return;
    }

    if (!checkBidirectionalRelationship(account, createGroupBody->members, res)) {
        marker.setReturnCode(res.result_int());
        return;
    }

    Group::EncryptStatus encryptFlag = Group::ENCRYPT_STATUS_OFF;
    if (!createGroupBody->ownerKey.empty()) {
        encryptFlag = Group::ENCRYPT_STATUS_ON;
    }

    auto createTime = nowInMilli();
    Group group;
    group.set_name(createGroupBody->name);
    group.set_icon(createGroupBody->icon);
    group.set_permission(0);
    group.set_updatetime(createTime);
    group.set_broadcast(static_cast<Group::BroadcastStatus>(createGroupBody->broadcast));
    group.set_status(1);
    group.set_channel("");
    group.set_intro(createGroupBody->intro);
    group.set_encryptstatus(encryptFlag);
    group.set_createtime(createTime);
    group.set_shareqrcodesetting(createGroupBody->qrCodeSetting);
    group.set_sharesignature(createGroupBody->shareSignature);
    group.set_ownerconfirm(createGroupBody->ownerConfirm);
    group.set_shareandownerconfirmsignature(createGroupBody->shareAndOwnerConfirmSignature);
    group.set_version(static_cast<int32_t>(group::GroupVersion::GroupV0));
    // check the group info is valid
    if (group.shareqrcodesetting().empty() 
        || group.sharesignature().empty() 
        || group.shareandownerconfirmsignature().empty()) {
        LOGE << "group setting verify failed, gid: " << group.gid() << ", owner confirm: " << group.ownerconfirm()
             << ", setting: " << group.shareqrcodesetting() << ", signature: " << group.shareandownerconfirmsignature();
        res.result(http::status::bad_request);
        marker.setReturnCode(res.result_int());
        return;
    }
    if (!verifyGroupSetting(group.shareqrcodesetting(), 
                            group.sharesignature(), 
                            group.ownerconfirm(),
                            group.shareandownerconfirmsignature(), 
                            account->publickey())) {
        LOGE << "group setting verify failed, gid: " << group.gid() << ", owner confirm: " << group.ownerconfirm()
             << ", setting: " << group.shareqrcodesetting() << ", signature: " << group.shareandownerconfirmsignature();
        res.result(http::status::bad_request);
        marker.setReturnCode(res.result_int());
        return;
    }

    uint64_t gid = 0;
    auto createGroupRet = m_groups->create(group, gid);
    if (createGroupRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to create group, data: " << group.Utf8DebugString();
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }

    LOGI << "create group success: " << account->uid() << ": " << gid << ": " << group.Utf8DebugString();

    // put all users into this group
    std::vector<GroupUser> users;
    const auto& members = createGroupBody->members;
    const auto& keys = createGroupBody->memberKeys;
    const auto& secret_keys = createGroupBody->membersGroupInfoSecrets;
    for (size_t i = 0; i < createGroupBody->members.size(); ++i) {
        GroupUser user;
        user.set_uid(members[i]);
        user.set_nick(members[i]);
        user.set_role(GroupUser::ROLE_MEMBER);
        user.set_gid(gid);
        user.set_lastackmid(0);
        user.set_status(static_cast<int32_t>(GroupUser::STATUS_DEFAULT));
        user.set_updatetime(createTime);
        user.set_createtime(createTime);
        if (encryptFlag == Group::ENCRYPT_STATUS_ON) {
            user.set_encryptedkey(keys[i]);
        }
        user.set_groupinfosecret(secret_keys[i]);
        users.push_back(std::move(user));
    }
    GroupUser owner;
    owner.set_uid(account->uid());
    owner.set_nick(account->uid());
    owner.set_nickname(createGroupBody->ownerNickname);
    owner.set_profilekeys(createGroupBody->ownerProfileKeys);
    owner.set_role(GroupUser::ROLE_OWNER);
    owner.set_gid(gid);
    owner.set_lastackmid(0);
    owner.set_status(static_cast<int32_t>(GroupUser::STATUS_DEFAULT));
    owner.set_updatetime(createTime);
    owner.set_createtime(createTime);
    if (encryptFlag == Group::ENCRYPT_STATUS_ON) {
        owner.set_encryptedkey(createGroupBody->ownerKey);
    }
    owner.set_groupinfosecret(createGroupBody->ownerSecretKey);
    users.push_back(std::move(owner));
    auto insertMembersRet = m_groupUsers->insertBatch(users);
    if (insertMembersRet != dao::ERRORCODE_SUCCESS) {
        m_groups->del(gid);
        LOGE << "failed to insert user into group, gid: " << group.gid();
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }

    LOGI << "insert group members success: " << account->uid() << ": " << gid << ": "
         << nlohmann::json(createGroupBody->members).dump();

    GroupSysMsgBody sysMsgBody;
    sysMsgBody.action = ENTER_GROUP;

    for (const auto& m : users) {
        publishGroupUserEvent(gid, m.uid(), INTERNAL_USER_ENTER_GROUP);
        if (m.uid() != account->uid()) {
            sysMsgBody.members.emplace_back(SimpleGroupMemberInfo{m.uid(), m.nick(), m.role()});
        }
    }

    pubGroupSystemMessage(account->uid(), gid, GROUP_MEMBER_UPDATE, nlohmann::json(sysMsgBody).dump());

    nlohmann::json result;
    result["gid"] = gid;
    result["failed_members"] = nlohmann::json::array();
    response.code = group::ERRORCODE_SUCCESS;
    response.msg = "success";
    response.result = result;
    context.responseEntity = response;
    res.result(http::status::ok);
    marker.setReturnCode(res.result_int());
}

void GroupManagerController::onUpdateGroupInfoV2(HttpContext& context)
{
    LOGT << "onUpdateGroupInfoV2 start...";
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onUpdateGroupInfoV2");
    auto& res = context.response;
    res.result(http::status::ok);

    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& statics = context.statics;
    statics.setUid(account->uid());

    auto* updateGroupBody = boost::any_cast<UpdateGroupInfoBodyV2>(&context.requestEntity);
    uint64_t gid = updateGroupBody->gid;
    GroupResponse response;
    bool checked = updateGroupBody->check(response);
    if (!checked) {
        LOGE << "parameter error, code: " << response.code << ", " << response.msg;
        res.result(http::status::bad_request);
        marker.setReturnCode(res.result_int());
        return;
    }

    GroupUser::Role role;
    auto roleRet = m_groupUsers->getMemberRole(gid, account->uid(), role);
    LOGD << "get member role: " << account->uid() << ": " << gid << ": " << roleRet << ": " << role;
    if (roleRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to get role, gid: " << gid << ", uid:" << account->uid() << ", error:" << roleRet;
        if (roleRet == dao::ERRORCODE_NO_SUCH_DATA) {
            res.result(http::status::not_found);
        } else {
            res.result(http::status::internal_server_error);
        }
        marker.setReturnCode(res.result_int());
        return;
    }

    if (role != GroupUser::ROLE_OWNER) {
        LOGE << "insufficient permission, gid: " << gid << ", uid:" << account->uid() << ", role:" << role;
        res.result(http::status::forbidden);
        marker.setReturnCode(res.result_int());
        return;
    }

    // special handling for qr code setting update
    if (!updateGroupBody->qrCodeSetting.empty()
        || updateGroupBody->ownerConfirm != -1) {
        // verify group setting to make sure the uploaded data are valid
        Group g;
        auto rc = m_groups->get(gid, g);
        if (rc != dao::ERRORCODE_SUCCESS) {
            LOGE << "failed to get group, gid: " << gid;
            res.result(http::status::internal_server_error);
            marker.setReturnCode(res.result_int());
            return;
        }
        std::string setting = (!updateGroupBody->qrCodeSetting.empty() ?
                updateGroupBody->qrCodeSetting : g.shareqrcodesetting());
        std::string settingSig = (!updateGroupBody->shareSignature.empty() ?
                updateGroupBody->shareSignature : g.sharesignature());
        int ownerConfirm = (updateGroupBody->ownerConfirm != -1 ? updateGroupBody->ownerConfirm : g.ownerconfirm());
        std::string shareAndOwnerConfirmSig = (!updateGroupBody->shareAndOwnerConfirmSignature.empty() ?
                updateGroupBody->shareAndOwnerConfirmSignature : g.shareandownerconfirmsignature());
        if (!verifyGroupSetting(setting,
                                settingSig,
                                ownerConfirm,
                                shareAndOwnerConfirmSig,
                                account->publickey())) {
            LOGE << "group setting verify failed, gid: " << gid << ", owner confirm: " << ownerConfirm
                 << ", setting: " << setting << ", signature: " << shareAndOwnerConfirmSig;
            res.result(http::status::bad_request);
            marker.setReturnCode(res.result_int());
            return;
        }
    }

    updateGroupBody->updateTime = nowInMilli();
    nlohmann::json upData = *updateGroupBody;
    LOGD << "update group data: " << account->uid() << ": " << gid << ": " << upData;

    auto upRet = m_groups->update(gid, upData);
    if (upRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to update group, gid: " << gid << ", data:" << upData.dump();
        res.result(http::status::forbidden);
        marker.setReturnCode(res.result_int());
        return;
    }

    LOGD << "update group table ok: " << account->uid() << ": " << gid;

    response.code = group::ERRORCODE_SUCCESS;
    response.msg = "success";
    context.responseEntity = response;
    res.result(http::status::ok);
    marker.setReturnCode(res.result_int());

    if (!updateGroupBody->qrCodeSetting.empty()) {
        // clean all pending list after changing to a new code, if the clean failed we just skip it
        // because those request wont be accepted latter because they can not pass the validation
        auto rc = m_pendingGroupUsers->clear(gid);
        if (rc != dao::ERRORCODE_SUCCESS) {
            LOGE << "failed to clean pending group users: gid " << gid;
        }
    }

    Group group;
    auto queryRet = m_groups->get(gid, group);
    if (queryRet != dao::ERRORCODE_SUCCESS) {
        // do not return error in this case because we have alredy updated the group successfully
        LOGE << "failed to get group, gid: " << gid << ", error: " << queryRet;
        return;
    }

    nlohmann::json msg = group;
    msg["owner"] = account->uid();
    msg.erase("permission");
    msg.erase("gid");
    msg.erase("status");
    msg.erase("encrypted_notice");

    pubGroupSystemMessage(account->uid(), gid, GROUP_INFO_UPDATE, msg.dump());
}

void GroupManagerController::onInviteGroupMemberV2(HttpContext& context)
{
    LOGT << "onInviteGroupMemberV2 start...";
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onInviteGroupMemberV2");
    auto &res = context.response;
    res.result(http::status::ok);

    auto *account = boost::any_cast<Account>(&context.authResult);
    auto &statics = context.statics;
    statics.setUid(account->uid());

    if (m_multiDeviceConfig.needUpgrade) {
        auto authDevice = *AccountsManager::getAuthDevice(*account);
        auto& clientVer = authDevice.clientversion();
        std::string feature_string = AccountsManager::getFeatures(*account, authDevice.id());
        BcmFeatures features(HexEncoder::decode(feature_string));
        if (clientVer.ostype() == ClientVersion::OSTYPE_IOS  || clientVer.ostype() == ClientVersion::OSTYPE_ANDROID) {
            if(!features.hasFeature(bcm::Feature::FEATURE_MULTI_DEVICE)) {
                LOGW << "onInviteGroupMemberV2: "<< "dont support multi device: " << account->uid();
                res.result(static_cast<unsigned>(custom_http_status::upgrade_requried));
                return;
            }
        }
    }

    auto *inviteGroupMemberBody = boost::any_cast<InviteGroupMemberBodyV2>(&context.requestEntity);
    uint64_t gid = inviteGroupMemberBody->gid;
    GroupResponse response;

    bool checked = inviteGroupMemberBody->check(response);
    if (!checked) {
        LOGE << "parameter error, code: " << response.code << ", " << response.msg;
        res.result(http::status::bad_request);
        marker.setReturnCode(res.result_int());
        return;
    }

    if (account->uid() != "bcm_backend_manager") {
        GroupUser::Role role;
        auto roleRet = m_groupUsers->getMemberRole(gid, account->uid(), role);
        LOGD << "get member role: " << account->uid() << ": " << gid << ": " << roleRet << ": " << role;
        if (roleRet != dao::ERRORCODE_SUCCESS) {
            LOGE << "failed to get role, gid: " << gid << ", uid:" << account->uid() << ", error:" << roleRet;
            if (roleRet == dao::ERRORCODE_NO_SUCH_DATA) {
                res.result(http::status::not_found);
            } else {
                res.result(http::status::internal_server_error);
            }
            marker.setReturnCode(res.result_int());
            return;
        }
        if (role != GroupUser::ROLE_OWNER && role != GroupUser::ROLE_MEMBER && role != GroupUser::ROLE_ADMINISTROR) {
            LOGE << "insufficient permission, gid: " << gid << ", uid:" << account->uid() << ", role:" << role;
            res.result(http::status::forbidden);
            marker.setReturnCode(res.result_int());
            return;
        }

        const std::vector<std::string>& members = inviteGroupMemberBody->members;
        if (!checkBidirectionalRelationshipBypassSubscribers(account, gid, members, res)) {
            marker.setReturnCode(res.result_int());
            return;
        }
    }

    Group group;
    auto queryRet = m_groups->get(gid, group);
    if (queryRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to get group, code: " << queryRet;
        if (queryRet == dao::ERRORCODE_NO_SUCH_DATA) {
            res.result(http::status::not_found);
        } else {
            res.result(http::status::internal_server_error);
        }
        marker.setReturnCode(res.result_int());
        return;
    }

    std::string ownerUid;
    dao::ErrorCode ec = m_groupUsers->getGroupOwner(gid, ownerUid);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "group owner data corrupted, gid: " << gid << ", error: " << ec;
        marker.setReturnCode(group::ERRORCODE_INTERNAL_ERROR);
        res.result(http::status::internal_server_error);
        return;
    }
    // ignore group setting check
    /*
    bcm::Account owner;
    if (!getGroupOwner(group.gid(), owner)) {
        LOGE << "failed to get owner, gid: " << group.gid();
        marker.setReturnCode(group::ERRORCODE_INTERNAL_ERROR);
        res.result(http::status::internal_server_error);
        return;
    }
    // check the data consistency to make sure the settings in database are correct
    if (!verifyGroupSetting(group.shareqrcodesetting(),
                            group.sharesignature(),
                            group.ownerconfirm(),
                            group.shareandownerconfirmsignature(),
                            owner.publickey())) {
        LOGE << "group setting verify failed, gid: " << group.gid() << ", owner confirm: " << group.ownerconfirm()
             << ", setting: " << group.shareqrcodesetting() << ", signature: " << group.shareandownerconfirmsignature();
        res.result(http::status::bad_request);
        return;
    }
     */


    int32_t ownerConfirm = group.ownerconfirm();
    bool encrypt = false;
    if (!!ownerConfirm && ownerUid != account->uid()) {
        // check data, signature
        if (inviteGroupMemberBody->members.size() != inviteGroupMemberBody->signatureInfos.size()) {
            LOGE << "invalid signature, member size: " << inviteGroupMemberBody->members.size()
                 << ", signature size: " << inviteGroupMemberBody->signatureInfos.size();
            res.result(http::status::bad_request);
            marker.setReturnCode(res.result_int());
            return;
        }
        for (const auto& item : inviteGroupMemberBody->signatureInfos) {
            std::string decoded = Base64::decode(item);
            nlohmann::json j = nlohmann::json::parse(decoded);
            decoded = Base64::decode(j.at("data").get<std::string>());
            if (!AccountHelper::verifySignature(account->publickey(), decoded, j.at("sig").get<std::string>())) {
                LOGE << "invalid signature: " << item;
                res.result(http::status::bad_request);
                marker.setReturnCode(res.result_int());
                return;
            }
        }
    } else {
        if (group.encryptstatus() == Group::ENCRYPT_STATUS_ON) {
            encrypt = true;
            if (inviteGroupMemberBody->members.size() != inviteGroupMemberBody->memberKeys.size()
                || inviteGroupMemberBody->members.size() != inviteGroupMemberBody->memberGroupInfoSecrets.size()) {
                LOGE << "开启加密群成员个数与密钥个数不一致, gid: " << std::to_string(gid);
                res.result(http::status::bad_request);
                marker.setReturnCode(res.result_int());
                return;
            }
        }
    }

    auto createTime = nowInMilli();
    auto updateTime = createTime;

    std::map<std::string, bcm::GroupUser::Role> roleMap;
    for (const auto &m : inviteGroupMemberBody->members) {
        roleMap[m] = GroupUser::ROLE_UNDEFINE;
    }
    auto roleMapRet = m_groupUsers->getMemberRoles(gid, roleMap);
    if (roleMapRet != dao::ERRORCODE_SUCCESS
            && roleMapRet != dao::ERRORCODE_NO_SUCH_DATA) {
        LOGE << "failed to get member role, code: " << roleMapRet;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }

    // just for migrating all subscribers to members
    if (ownerUid == account->uid()) {
        std::vector<std::string> delList;
        for (const auto& r : roleMap) {
            if (r.second != bcm::GroupUser::ROLE_SUBSCRIBER) {
                continue;
            }
            delList.emplace_back(r.first);
        }
        if (!delList.empty()) {
            auto rc = m_groupUsers->delMemberBatch(gid, delList);
            if (dao::ERRORCODE_SUCCESS != rc && dao::ERRORCODE_NO_SUCH_DATA != rc) {
                LOGE << "failed to del members, code: " << rc;
                res.result(http::status::internal_server_error);
                marker.setReturnCode(res.result_int());
                return;
            }
        }
    }

    if (!!ownerConfirm && ownerUid != account->uid()) {//insert into pending_group_user
        std::vector<PendingGroupUser> pendingUsers;
        const auto &members    = inviteGroupMemberBody->members;
        const auto &signatures = inviteGroupMemberBody->signatureInfos;
        InviteGroupMemberResponse2 result;

        for (size_t i = 0; i < members.size(); i++) {
            const auto &uid = members[i];
            auto it = roleMap.find(uid);
            if (it != roleMap.end() && it->second != GroupUser::ROLE_UNDEFINE) {
                // already in group, ignore it
                continue;
            }
            PendingGroupUser pendingUser;
            pendingUser.set_gid(gid);
            pendingUser.set_uid(uid);
            pendingUser.set_signature(signatures[i]);
            pendingUser.set_inviter(account->uid());
            pendingUser.set_comment("");
            auto insertRet = m_pendingGroupUsers->set(pendingUser);
            if (insertRet != dao::ERRORCODE_SUCCESS) {
                LOGE << "failed to insert pending group user, error: " << insertRet;
                res.result(http::status::internal_server_error);
                marker.setReturnCode(res.result_int());
                return;
            }
            result.successMembers.emplace_back(pendingUser.uid(), pendingUser.uid(), GroupUser::ROLE_MEMBER);
        }

        response.result = result;
        response.code = group::ERRORCODE_SUCCESS;
        response.msg = "success";
        context.responseEntity = response;
        res.result(http::status::ok);
        marker.setReturnCode(res.result_int());

        pubReviewJoinRequestToOwner(group.gid(), ownerUid);
    } else {//insert into group_user table,
        std::vector<GroupUser> users;
        InviteGroupMemberResponse2 result;
        const auto& members = inviteGroupMemberBody->members;
        const auto& keys = inviteGroupMemberBody->memberKeys;
        const auto& groupInfoSecrets = inviteGroupMemberBody->memberGroupInfoSecrets;
        for (size_t i = 0; i < members.size(); ++i) {
            const auto &uid = members[i];
            if (roleMap.find(uid) != roleMap.end()) {
                if (roleMap[uid] != GroupUser::ROLE_UNDEFINE &&
                        roleMap[uid] <= GroupUser::ROLE_MEMBER) {
                    result.failedMembers.push_back(members[i]);
                } else {
                    GroupUser user;
                    user.set_uid(uid);
                    user.set_nick(uid);
                    user.set_role(GroupUser::ROLE_MEMBER);
                    user.set_gid(gid);
                    user.set_lastackmid(group.lastmid());
                    user.set_status(GroupUser::STATUS_DEFAULT);
                    user.set_updatetime(updateTime);
                    user.set_createtime(createTime);
                    if (encrypt) {
                        user.set_encryptedkey(keys[i]);
                    }
                    user.set_groupinfosecret(groupInfoSecrets[i]);
                    users.push_back(std::move(user));
                }
            } else {
                result.failedMembers.push_back(members[i]);
            }
        }

        if (users.empty()) {
            response.result = result;
            response.code = group::ERRORCODE_SUCCESS;
            response.msg = "success";
            context.responseEntity = response;
            res.result(http::status::ok);
            marker.setReturnCode(res.result_int());
            return;
        }

        auto insertRet = m_groupUsers->insertBatch(users);
        if (insertRet != dao::ERRORCODE_SUCCESS) {
            LOGE << "failed to insert pending group user, error: " << insertRet;
            res.result(http::status::internal_server_error);
            marker.setReturnCode(res.result_int());
            return;
        }

        for (const auto &m : users) {
            result.successMembers.emplace_back(m.uid(), m.uid(), m.role());
            if (roleMap[m.uid()] == GroupUser::ROLE_UNDEFINE) {
                publishGroupUserEvent(gid, m.uid(), INTERNAL_USER_ENTER_GROUP);
            } else {
                publishGroupUserEvent(gid, m.uid(), INTERNAL_USER_CHANGE_ROLE);
            }
        }

        response.result = result;
        response.code = group::ERRORCODE_SUCCESS;
        response.msg = "success";
        context.responseEntity = response;
        res.result(http::status::ok);
        marker.setReturnCode(res.result_int());

        GroupSysMsgBody msg{ENTER_GROUP, result.successMembers};
        pubGroupSystemMessage(account->uid(), gid, GROUP_MEMBER_UPDATE, nlohmann::json(msg).dump());
    }

}


void GroupManagerController::onIsQrCodeValid(HttpContext& context)
{
    LOGT << "onIsQrCodeValid start...";
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onIsQrCodeValid");
    auto& response = context.response;
    //auto* account = boost::any_cast<Account>(&context.authResult);

    QrCodeInfo* req = boost::any_cast<QrCodeInfo>(&context.requestEntity);
    std::string error;
    if (!req->check(error)) {
        LOGE << error;
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }
    bcm::Group group;
    dao::ErrorCode ec = m_groups->get(req->gid, group);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to get group, gid: " << req->gid << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }
    bcm::Account owner;
    std::string ownerUid;
    ec = m_groupUsers->getGroupOwner(group.gid(), ownerUid);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to get group owner uid, gid: " << req->gid << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }

    ec = m_accountsManager->get(ownerUid, owner);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to get group owner, ownerUid: " << ownerUid << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }
    // check the data consistency to make sure the settings in database are correct
    if (!verifyGroupSetting(group.shareqrcodesetting(),
                            group.sharesignature(),
                            group.ownerconfirm(),
                            group.shareandownerconfirmsignature(),
                            owner.publickey())) {
        LOGE << "group setting verify failed, gid: " << req->gid << ", owner confirm: " << group.ownerconfirm()
             << ", setting: " << group.shareqrcodesetting() << ", signature: " << group.shareandownerconfirmsignature();
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }
    GroupResponse response1;
    std::string decoded = Base64::decode(group.shareqrcodesetting());
    bool valid = AccountHelper::verifySignature(owner.publickey(), decoded, req->qrCodeToken);

    response.result(http::status::ok);
    response1.msg = "success";
    nlohmann::json msg;
    msg["valid"] = valid;
    response1.result = msg;
    response1.code = group::ErrorCode::ERRORCODE_SUCCESS;
    context.responseEntity = response1;
    marker.setReturnCode(response.result_int());
    return;
}

void GroupManagerController::onGetOwnerConfirm(HttpContext& context)
{
    LOGT << "onGetOwnerConfirm start...";
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onGetOwnerConfirm");
    auto& res = context.response;
    res.result(http::status::ok);

    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& statics = context.statics;
    statics.setUid(account->uid());

    GetOwnerConfirmInfo* pRequest = boost::any_cast<GetOwnerConfirmInfo>(&context.requestEntity);
    LOGI << "to get group owner confirm info.(" << " gid:" << pRequest->gid << ")";

    Group group;
    GroupResponse response;

    if (m_groups->get(pRequest->gid, group) != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to read group info.(" << " gid:"<< pRequest->gid << ")";
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }
    //response success.
    response.code          = group::ERRORCODE_SUCCESS;
    response.msg           = "success";
    nlohmann::json msg;
    msg["owner_confirm"] = group.ownerconfirm();
    response.result        = msg;
    context.responseEntity = response;
    res.result(http::status::ok);
    marker.setReturnCode(res.result_int());
    LOGI << "success to get owner confirm info";

}

void GroupManagerController::pubSecretRefreshToUser(uint64_t gid,
                                                    const std::string& uid,
                                                    const std::string& msgKey,
                                                    const std::string& groupInfoKey)
{
    auto address = DispatchAddress(uid, bcm::Device::MASTER_ID);
    GroupSecretRefresh content;
    content.set_gid(gid);
    content.set_uid(uid);
    content.set_groupinfosecret(groupInfoKey);
    content.set_messagesecret(msgKey);
    PubSubMessage pubMessage;
    GroupMsgOut out;
    out.set_type(GroupMsg::TYPE_KEY_REFRESH);
    out.set_body(content.SerializeAsString());
    pubMessage.set_type(PubSubMessage::NOTIFICATION);
    pubMessage.set_content(out.SerializeAsString());

    OnlineRedisManager::Instance()->publish(address.getUid(), address.getSerialized(), pubMessage.SerializeAsString(),
                                            [address](int status, const redis::Reply& reply) {
                                                if (REDIS_OK != status || !reply.isInteger()) {
                                                    LOGE << "publish pubSecretRefreshToUser message failed for uid: "
                                                         << address.getUid()
                                                         << ", status: " << status;
                                                    return;
                                                }
    
                                                if (reply.getInteger() > 0) {
                                                    LOGI << "success to pubSecretRefreshToUser message to redis, uid: "
                                                            << address.getUid() << ", subscribe: " << reply.getInteger();
                                                } else {
                                                    LOGE << "failed to pubSecretRefreshToUser message to redis, uid: "
                                                            << address.getUid() << ", subscribe: " << reply.getInteger();
                                                }
                                            });
}

void GroupManagerController::pubReviewJoinRequestToOwner(uint64_t gid, const std::string& uid)
{
    auto address = DispatchAddress(uid, bcm::Device::MASTER_ID);
    GroupJoinReviewRequest content;
    content.set_gid(gid);
    PubSubMessage pubMessage;
    GroupMsgOut out;
    out.set_type(GroupMsg::TYPE_JOIN_REVIEW);
    out.set_body(content.SerializeAsString());
    pubMessage.set_type(PubSubMessage::NOTIFICATION);
    pubMessage.set_content(out.SerializeAsString());
    
    OnlineRedisManager::Instance()->publish(address.getUid(), address.getSerialized(), pubMessage.SerializeAsString(),
                                            [address](int status, const redis::Reply& reply) {
                                                if (REDIS_OK != status || !reply.isInteger()) {
                                                    LOGE << "publish pubSecretRefreshToUser message failed for uid: "
                                                         << address.getUid()
                                                         << ", status: " << status;
                                                    return;
                                                }
        
                                                if (reply.getInteger() > 0) {
                                                    LOGI << "success to pubSecretRefreshToUser message to redis, uid: "
                                                         << address.getUid() << ", subscribe: " << reply.getInteger();
                                                } else {
                                                    LOGE << "failed to pubSecretRefreshToUser message to redis, uid: "
                                                         << address.getUid() << ", subscribe: " << reply.getInteger();
                                                }
                                            });

}

void GroupManagerController::selectKeyDistributionCandidates(uint64_t gid,
                                                             uint seed,
                                                             size_t count,
                                                             OnlineMsgMemberMgr::UserSet& candidates)
{
    candidates.clear();
    if (count == 0) {
        return;
    }
    OnlineMsgMemberMgr::UserList users;
    m_groupMsgService->getLocalOnlineGroupMembers(gid, 0, users);
    if (users.size() <= count) {
        candidates.insert(users.begin(), users.end());
        return;
    }
    srand(seed);
    int index = rand() % users.size();
    do {
        // for now, only master device can be selected
        if (users[index].getDeviceid() == Device::MASTER_ID) {
            candidates.emplace(users[index]);
            index = (index + 1) % users.size();
        }
    } while (candidates.size() < count);
}

void GroupManagerController::onQueryMemberListOrdered(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName,
        "onQueryMemberListOrdered");

    QueryMemberListOrderedBody* request = boost::any_cast<QueryMemberListOrderedBody>(&context.requestEntity);
    auto& response = context.response;
    Account* account = boost::any_cast<Account>(&context.authResult);

    std::string sep("");
    std::stringstream ss;
    ss << "[";
    for (int r : request->roles) {
        ss << sep << r;
        sep = ",";
    }
    ss << "]";

    LOGI << "receive to query group member list.(uid:" << account->uid()
         << " gid:" << request->gid << " role:" << ss.str() << ")";
    
    std::string error;
    if (!request->check(error)) {
#ifndef NDEBUG
        context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        LOGE << error; 
        return;
    }

    GroupUser user;
    dao::ErrorCode ec = m_groupUsers->getMember(request->gid, account->uid(), user);
    if (dao::ERRORCODE_NO_SUCH_DATA == ec) {
#ifndef NDEBUG
        context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, "group not existed or you are not in this group");
#endif
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        LOGE << "there is no user " << account->uid() << " in group " << request->gid; 
        return;
    }
    if (dao::ERRORCODE_SUCCESS != ec) {
#ifndef NDEBUG
        context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, "database error when getting member info for uid " + account->uid());
#endif
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        LOGE << "failed to query my group user info.(myuid:" << account->uid()
             << " gid:"<< request->gid << "): " << ec;
        return;
    }

    std::vector<GroupUser::Role> roles;
    roles.reserve(request->roles.size());
    for (int r : request->roles) {
        roles.push_back(GroupUser::Role(r));
    }
    std::vector<GroupUser> members;
    ec = m_groupUsers->getMembersOrderByCreateTime(request->gid, 
                                                   roles,
                                                   request->startUid,
                                                   request->createTime,
                                                   request->count,
                                                   members);
    if (dao::ERRORCODE_NO_SUCH_DATA != ec && dao::ERRORCODE_SUCCESS != ec) {
#ifndef NDEBUG
        context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, "database error when getting members for this group");
#endif
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        LOGE << "failed to read group member list.(myuid:" << account->uid()
             << " gid:"<< request->gid << "): " << ec;
        return;
    }

    nlohmann::json memberList = nlohmann::json::array();
    for (auto it = members.begin(); it != members.end(); ++it) {
        nlohmann::json m = nlohmann::json{
            {"uid", it->uid()},
            {"nick", it->nick()},
            {"nickname", it->nickname()},
            {"groupNickname", it->groupnickname()},
            {"profileKeys", it->profilekeys()},
            {"role", it->role()},
            {"createTime", it->createtime()},
            {"proof", it->proof()}
        };
        memberList.push_back(m);
    }

    GroupResponse resBody(group::ERRORCODE_SUCCESS, "");
    resBody.result = nlohmann::json{{"members", memberList}};
    context.responseEntity = resBody;
    response.result(http::status::ok);
    marker.setReturnCode(response.result_int());

    LOGT << "success to query group member list.(" << resBody.result.dump() << ")";
}

bool GroupManagerController::checkBidirectionalRelationship(Account* account, 
    const std::vector<std::string>& members, http::response<http::string_body>& resp)
{
    if (account->has_contactsfilters()) {
        const ContactsFilters& filterContent = account->contactsfilters();
        std::string decodedContent = Base64::decode(filterContent.content());
        BloomFilters bloomFilter(filterContent.algo(), decodedContent);
        for (auto it = members.begin(); it != members.end(); ++it) {
            if (!bloomFilter.contains(*it)) {
                LOGE << "member " << *it << " is not a friend of " << account->uid();
                // NOTE: "200 ok" is returned when this check failed since we want to 
                // hide the truth from attackers.
                resp.result(http::status::ok);
                return false;
            }
        }
    }

    for (auto it = members.begin(); it != members.end(); ++it) {
        Account memberAccount;
        dao::ErrorCode ec = m_accountsManager->get(*it, memberAccount);
        if (ec != dao::ERRORCODE_SUCCESS) {
            LOGE << "error loading account, uid: " << *it << ", ec: " << ec;
            resp.result(http::status::internal_server_error);
            return false;
        }
        if (memberAccount.has_contactsfilters()) {
            const ContactsFilters& filterContent = memberAccount.contactsfilters();
            std::string decodedContent = Base64::decode(filterContent.content());
            BloomFilters bloomFilter(filterContent.algo(), decodedContent);
            if (!bloomFilter.contains(account->uid())) {
                LOGE << "member " << account->uid() << " is not a friend of " << *it;
                // NOTE: "200 ok" is returned when this check failed since we want to 
                // hide the truth from attackers.
                resp.result(http::status::ok);
                return false;
            }
        }
    }

    return true;
}

bool GroupManagerController::checkBidirectionalRelationshipBypassSubscribers(
    Account* account, uint64_t gid, const std::vector<std::string>& members,
    http::response<http::string_body>& resp)
{
    std::map <std::string, bcm::GroupUser::Role> memberRoles;
    for (auto it = members.begin(); it != members.end(); ++it) {
        memberRoles.emplace(*it, GroupUser::ROLE_UNDEFINE);
    }
    dao::ErrorCode ec = m_groupUsers->getMemberRoles(gid, memberRoles);
    if (ec != dao::ERRORCODE_SUCCESS && ec != dao::ERRORCODE_NO_SUCH_DATA) {
        LOGE << "error loading member roles, gid: " << gid << ", ec: " << ec;
        resp.result(http::status::internal_server_error);
        return false;
    }

    if (account->has_contactsfilters()) {
        const ContactsFilters& filterContent = account->contactsfilters();
        std::string decodedContent = Base64::decode(filterContent.content());
        BloomFilters bloomFilter(filterContent.algo(), decodedContent);
        for (auto it = members.begin(); it != members.end(); ++it) {
            auto itRole = memberRoles.find(*it);
            if (itRole != memberRoles.end()) {
                if (itRole->second == GroupUser::ROLE_SUBSCRIBER) {
                    LOGD << "member " << *it << " is a subscriber, let him join";
                    continue;
                }
            }
            if (!bloomFilter.contains(*it)) {
                LOGE << "member " << *it << " is not a friend of " << account->uid();
                // NOTE: "200 ok" is returned when this check failed since we want to 
                // hide the truth from attackers.
                resp.result(http::status::ok);
                return false;
            } else {
                LOGD << "member " << *it << " is a friend of " << account->uid() 
                        << ", let him join";
            }
        }
    } else {
        LOGD << "account " << account->uid() << " does not have a contact filter";
    }

    for (auto it = members.begin(); it != members.end(); ++it) {
        auto itRole = memberRoles.find(*it);
        if (itRole != memberRoles.end()) {
            if (itRole->second == GroupUser::ROLE_SUBSCRIBER) {
                continue;
            }
        }
        Account memberAccount;
        dao::ErrorCode ec = m_accountsManager->get(*it, memberAccount);
        if (ec != dao::ERRORCODE_SUCCESS) {
            LOGE << "error loading account, uid: " << *it << ", ec: " << ec;
            resp.result(http::status::internal_server_error);
            return false;
        }
        if (memberAccount.has_contactsfilters()) {
            const ContactsFilters& filterContent = memberAccount.contactsfilters();
            std::string decodedContent = Base64::decode(filterContent.content());
            BloomFilters bloomFilter(filterContent.algo(), decodedContent);
            if (!bloomFilter.contains(account->uid())) {
                LOGE << "member " << account->uid() << " is not a friend of " << *it;
                // NOTE: "200 ok" is returned when this check failed since we want to 
                // hide the truth from attackers.
                resp.result(http::status::ok);
                return false;
            } else {
                LOGD << "account " << account->uid() << " is a friend of " << *it 
                        << ", let him join";
            }
        } else {
            LOGD << "account " << memberAccount.uid() << " does not have a contact filter";
        }
    }

    return true;
}

void GroupManagerController::onSetGroupExtensionInfo(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onSetGroupExtensionInfo");
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    GroupExtensionInfo* groupExtensionInfo = boost::any_cast<GroupExtensionInfo>(&context.requestEntity);
    LOGI << "onSetGroupExtensionInfo, uid: " << account->uid() << ", gid: "
         << groupExtensionInfo->gid << ", keys size: " << groupExtensionInfo->extensions.size();

    // check the group user is owner
    GroupUser::Role role;
    auto getMemberRet = m_groupUsers->getMemberRole(groupExtensionInfo->gid, account->uid(), role);
    if (getMemberRet == dao::ERRORCODE_NO_SUCH_DATA) {
        LOGE << "the uid is not group members, uid: " << account->uid()
             << ", gid: " << groupExtensionInfo->gid;
        res.result(static_cast<unsigned>(bcm::custom_http_status::not_group_owner));
        marker.setReturnCode(res.result_int());
        return;
    }
    if (getMemberRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to query group user info, uid: " << account->uid()
             << ", gid: " << groupExtensionInfo->gid;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }
    if (role != GroupUser::ROLE_OWNER) {
        LOGE << "failed to query group user not owner, uid: " << account->uid()
             << ", gid: " << groupExtensionInfo->gid;
        res.result(static_cast<unsigned>(bcm::custom_http_status::not_group_owner));
        marker.setReturnCode(res.result_int());
        return;
    }

    if (groupExtensionInfo->extensions.size() > 256) {
        LOGE << "Too much information, uid: " << account->uid()
             << ", gid: " << groupExtensionInfo->gid
             << ", key size: " << groupExtensionInfo->extensions.size();
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }
    for (const auto& itInfo : groupExtensionInfo->extensions) {
        if (itInfo.first.size() > 256 || itInfo.second.size() > 128 * 1024) {
            LOGE << "Too much information, uid: " << account->uid()
                 << ", gid: " << groupExtensionInfo->gid
                 << ", key size: " << itInfo.first.size() << ", value size: " << itInfo.second.size();
            res.result(http::status::internal_server_error);
            marker.setReturnCode(res.result_int());
        }
    }

    getMemberRet = m_groups->setGroupExtensionInfo(groupExtensionInfo->gid, groupExtensionInfo->extensions);
    if (getMemberRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to query group user info, uid: " << account->uid()
             << ", gid: " << groupExtensionInfo->gid;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }

    res.result(http::status::ok);
    marker.setReturnCode(res.result_int());
    return;
}

void GroupManagerController::onGetGroupExtensionInfo(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onGetGroupExtensionInfo");
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    QueryGroupExtensionInfo* groupExtensionReq = boost::any_cast<QueryGroupExtensionInfo>(&context.requestEntity);
    LOGI << "onGetGroupExtensionInfo, uid: " << account->uid() << ", gid: "
         << groupExtensionReq->gid << ", keys: " << bcm::toString(groupExtensionReq->extensionKeys);

    // check whether the user in this group
    GroupUser::Role role;
    auto getMemberRet = m_groupUsers->getMemberRole(groupExtensionReq->gid, account->uid(), role);
    if (getMemberRet == dao::ERRORCODE_NO_SUCH_DATA) {
        LOGE << "the uid is not group members, uid: " << account->uid()
             << ", gid: " << groupExtensionReq->gid;
        res.result(static_cast<unsigned>(bcm::custom_http_status::not_group_user));
        marker.setReturnCode(res.result_int());
        return;
    }
    if (getMemberRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to query group user info, uid: " << account->uid()
             << ", gid: " << groupExtensionReq->gid;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }
    GroupExtensionInfo groupExtensionInfo;
    groupExtensionInfo.gid = groupExtensionReq->gid;
    getMemberRet = m_groups->getGroupExtensionInfo(groupExtensionReq->gid,
                                                   groupExtensionReq->extensionKeys,
                                                   groupExtensionInfo.extensions);
    context.responseEntity = groupExtensionInfo;
    res.result(http::status::ok);
    marker.setReturnCode(res.result_int());
    return;
}

} // namespace bcm
