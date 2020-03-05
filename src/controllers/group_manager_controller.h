#pragma once
#include <memory>
#include <boost/beast/http/status.hpp>
#include "http/http_router.h"
#include "proto/dao/group_user.pb.h"
#include "proto/dao/group_keys.pb.h"
#include "proto/dao/group.pb.h"
#include "group/message_type.h"
#include "dao/group_users.h"
#include "dao/group_msgs.h"
#include "dao/groups.h"
#include "dao/group_keys.h"
#include "dao/pending_group_users.h"
#include "dao/accounts.h"
#include "group/group_msg_service.h"
#include "config/group_config.h"
#include "config/multi_device_config.h"

#include "dispatcher/dispatch_manager.h"
#include "group_manager_entities.h"
#include "limiters/limiter.h"
#include "dao/dao_cache/redis_cache.h"

#ifdef UNIT_TEST
#define private public
#define protected public
#endif


namespace bcm {

class GroupManagerController
    : public HttpRouter::Controller
    , public std::enable_shared_from_this<GroupManagerController> {

public:

    GroupManagerController(
        std::shared_ptr<bcm::GroupMsgService> groupMsgService,
        std::shared_ptr<bcm::AccountsManager> accountsManager,
        std::shared_ptr<bcm::DispatchManager> dispatchManager,
        const MultiDeviceConfig& multiDeviceCfg,
        const GroupConfig& groupConfig);

    ~GroupManagerController() = default;

public:
    void addRoutes(HttpRouter& router) override;

    void onCreateGroup(HttpContext& context);
    void onCreateGroupV2(HttpContext& context);
    void onUpdateGroupInfo(HttpContext& context);
    void onInviteGroupMember(HttpContext& context);
    void onInviteGroupMemberV2(HttpContext& context);
    void onKickGroupMember(HttpContext& context);
    void onLeaveGroup(HttpContext& context);
    void onSubscribeGroup(HttpContext& context);
    void onUnsubscribeGroup(HttpContext& context);
    void onQueryGroupInfo(HttpContext& context);
    void onQueryGroupInfoBatch(HttpContext& context);
    void onQueryJoinGroupList(HttpContext& context);
    void onQueryGroupMemberList(HttpContext& context);
    void onQueryGroupMemberListSegment(HttpContext& context);
    void onQueryGroupMemberInfo(HttpContext& context);
//    void onQuitJoinedGroups(HttpContext& context);
    void onUpdateGroupUserInfo(HttpContext& context);
    void onUpdateGroupNotice(HttpContext& context);
    void onGroupJoinRequst(HttpContext& context);
    void onQueryGroupPendingList(HttpContext& context);
    void onReviewJoinRequest(HttpContext& context);
    void onUploadPassword(HttpContext& context);
    void onUpdateGroupInfoV2(HttpContext& context);
    void onGetOwnerConfirm(HttpContext& context);
    void onIsQrCodeValid(HttpContext& context);
    void onQueryMemberListOrdered(HttpContext& context);
    void onSetGroupExtensionInfo(HttpContext& context);
    void onGetGroupExtensionInfo(HttpContext& context);

    // V3
    void onCreateGroupV3(HttpContext& context);
    void onFetchGroupKeysV3(HttpContext& context);
    void onLeaveGroupV3(HttpContext& context);
    void onKickGroupMemberV3(HttpContext& context);
    void fireGroupKeysUpdateV3(HttpContext& context);
    void onUploadGroupKeysV3(HttpContext& context);
    void onDhKeysV3(HttpContext& context);
    void onPrepareKeyUpdateV3(HttpContext& context);
    void onGroupJoinRequstV3(HttpContext& context);
    void onAddMeRequstV3(HttpContext& context);
    void onInviteGroupMemberV3(HttpContext& context);
    void onReviewJoinRequestV3(HttpContext& context);
    void onUpdateGroupInfoV3(HttpContext& context);
    void onFetchLatestGroupKeysV3(HttpContext& context);
    void onQueryMembersV3(HttpContext& context);

private:
 bool pubGroupSystemMessage(const std::string& uid, const uint64_t groupid, const int type, const std::string& strText);

 bool pubGroupSystemMessage(const std::string& uid, 
                            const uint64_t groupid, 
                            const int type, 
                            const std::string& strText,
                            uint64_t& mid);

 bool publishGroupUserEvent(const uint64_t& gid, const std::string& uid, const InternalMessageType& type);

 bool getGroupOwner(uint64_t gid, bcm::Account& owner);

 bool verifyGroupSetting(const std::string& qrCodeSetting, const std::string& shareSignature, int ownerConfirm,
                         const std::string& shareAndOwnerConfrimSignature, const std::string& ownerPublicKey);

 void pubSecretRefreshToUser(uint64_t gid, const std::string& uid, const std::string& msgKey,
                             const std::string& groupInfoKey);

 void pubReviewJoinRequestToOwner(uint64_t gid, const std::string& uid);

 void selectKeyDistributionCandidates(uint64_t gid, uint seed, size_t count, OnlineMsgMemberMgr::UserSet& candidates);

 void distributeKeys(HttpContext& context, const ReviewJoinResultList* req, const bcm::Group& group);

 bool checkBidirectionalRelationship(Account* account, const std::vector<std::string>& members,
                                     http::response<http::string_body>& resp);
 bool checkBidirectionalRelationshipBypassSubscribers(Account* account, uint64_t gid,
                                                      const std::vector<std::string>& members,
                                                      http::response<http::string_body>& resp);

 bool sendGroupKeysUpdateRequestWhenMemberChanges(const std::string& uid, uint64_t gid,
                                                  uint32_t groupMembersAfterChanges);

 bool sendGroupKeysUpdateRequestWithRetry(const std::string& uid, uint64_t gid, int32_t groupKeysMode);

 bool sendSwitchGroupKeysWithRetry(const std::string& uid, uint64_t gid, int64_t version);

 std::string getMemberJoinLimiterId(uint64_t gid, const std::string& uid);

 int32_t random(int32_t min, int32_t max);

protected:
    class KeysCache {
    public:
        KeysCache(int64_t ttl) : m_cache(new dao::RedisCache(ttl)) {}
        bool get(uint64_t gid, int64_t version, std::vector<bcm::Keys>& keys);
        bool set(uint64_t gid, int64_t version, const std::vector<bcm::Keys>& keys);
    private:
        std::string cacheKey(uint64_t gid, int64_t version);
        bool serialize(const std::vector<bcm::Keys>& keys, std::string& value);
        bool deserialize(const std::string& value, std::vector<bcm::Keys>& keys);
    private:
        std::shared_ptr<bcm::dao::RedisCache> m_cache;
    };

private:
    std::shared_ptr<bcm::dao::Groups> m_groups;
    std::shared_ptr<bcm::dao::GroupUsers> m_groupUsers;
    std::shared_ptr<bcm::dao::GroupKeys> m_groupKeys;
    std::shared_ptr<bcm::dao::GroupMsgs> m_groupMessage;
    std::shared_ptr<bcm::dao::PendingGroupUsers> m_pendingGroupUsers;
    std::shared_ptr<bcm::dao::QrCodeGroupUsers> m_qrCodeGroupUsers;
    std::shared_ptr<bcm::GroupMsgService> m_groupMsgService;

    std::shared_ptr<ILimiter> m_dhKeysLimiter;
    std::shared_ptr<ILimiter> m_groupCreationLimiter;
    std::shared_ptr<ILimiter> m_groupFireKeysUpdateLimiter;
    std::shared_ptr<ILimiter> m_groupMemberJoinLimiter;
    std::shared_ptr<bcm::AccountsManager> m_accountsManager;
    std::shared_ptr<bcm::DispatchManager> m_dispatchManager;

    MultiDeviceConfig m_multiDeviceConfig;
    GroupConfig m_groupConfig;
    KeysCache m_keysCache;
};

} // namespace bcm

#ifdef UNIT_TEST
#undef private
#undef protected
#endif


