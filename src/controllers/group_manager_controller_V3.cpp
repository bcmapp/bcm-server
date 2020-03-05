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
#include "metrics_client.h"
#include "config/group_store_format.h"

#include "redis/redis_manager.h"
#include "redis/online_redis_manager.h"
#include "redis/reply.h"
#include "store/accounts_manager.h"
#include "crypto/base64.h"
#include "http/custom_http_status.h"
#include "features/bcm_features.h"
#include "crypto/hex_encoder.h"

#include <boost/fiber/all.hpp>
#include <chrono>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>


namespace bcm {
namespace http = boost::beast::http;

using namespace metrics;

constexpr char kMetricsGmanagerServiceName[] = "group_manager";
constexpr int64_t kQrCodeGroupUserTtl = 60;

static inline bool checkGroupVersion(const bcm::Group& group, group::GroupVersion expeted)
{
    if (group.version() != static_cast<int32_t>(expeted)) {
        LOGE << "group version mismatch, expeted: " << expeted << ", actual: " << group.version();
        return false;
    }
    return true;
}

void GroupManagerController::onUploadGroupKeysV3(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onUploadGroupKeysV3");
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto* uploadGroupKeysRequest = boost::any_cast<UploadGroupKeysRequest>(&context.requestEntity);
    LOGI << "onUploadGroupKeysV3, uid: " << account->uid() << ", gid: "
         << uploadGroupKeysRequest->gid << ", groupKeysMode: " << uploadGroupKeysRequest->groupKeysMode;

#ifdef GROUP_EXCEPTION_INJECT_TEST
    if (m_groupConfig.groupConfigExceptionInject.uploadGroupKeysException) {
        LOGI << "USED GROUP_EXCEPTION_INJECT_TEST, uploadGroupKeysException, uid: " << account->uid() << ", gid: "
         << uploadGroupKeysRequest->gid << ", groupKeysMode: " << uploadGroupKeysRequest->groupKeysMode
         << ", mock code: " << m_groupConfig.groupConfigExceptionInject.uploadGroupKeysExceptionCode;

        res.result(static_cast<unsigned>(m_groupConfig.groupConfigExceptionInject.uploadGroupKeysExceptionCode));
        marker.setReturnCode(res.result_int());
        return;
    } else if (m_groupConfig.groupConfigExceptionInject.randomUploadGroupKeysException) {
        int32_t percent = nowInMilli() % 100;
        do {
            if (percent <= m_groupConfig.groupConfigExceptionInject.randomUploadGroupKeysSuccessPercent) {
                // NO-OP
                LOGI << "randomUploadGroupKeysException, success! uid: " << account->uid()
                     << ", gid: " << uploadGroupKeysRequest->gid << ", groupKeysMode: " << uploadGroupKeysRequest->groupKeysMode;
                break;
            }
            percent -= m_groupConfig.groupConfigExceptionInject.randomUploadGroupKeysSuccessPercent;
            if (percent <= m_groupConfig.groupConfigExceptionInject.randomUploadGroupKeysDelayPercent) {
                int32_t delayTimeInMills = random(m_groupConfig.groupConfigExceptionInject.randomUploadGroupKeysDelayMinInMills,
                                                  m_groupConfig.groupConfigExceptionInject.randomUploadGroupKeysDelayMaxInMills);
                LOGI << "randomUploadGroupKeysException, delay! random time in mills: " << delayTimeInMills << ", uid: " << account->uid()
                     << ", gid: " << uploadGroupKeysRequest->gid << ", groupKeysMode: " << uploadGroupKeysRequest->groupKeysMode;
                boost::this_fiber::sleep_for(std::chrono::milliseconds(delayTimeInMills));
                break;
            }
            percent -= m_groupConfig.groupConfigExceptionInject.randomUploadGroupKeysDelayPercent;
            if (percent <= m_groupConfig.groupConfigExceptionInject.randomUploadGroupKeysExceptionPercent) {
                LOGI << "randomUploadGroupKeysException, exception! uid: " << account->uid()
                     << ", gid: " << uploadGroupKeysRequest->gid << ", groupKeysMode: " << uploadGroupKeysRequest->groupKeysMode;
                res.result(http::status::internal_server_error);
                marker.setReturnCode(res.result_int());
                return;
            }
        } while (0);
    }
#endif

    // check whether the user in this group
    GroupUser user;
    auto getMemberRet = m_groupUsers->getMember(uploadGroupKeysRequest->gid, account->uid(), user);
    if (getMemberRet == dao::ERRORCODE_NO_SUCH_DATA) {
        LOGE << "the uid is not group members, uid: " << account->uid()
             << ", gid: " << uploadGroupKeysRequest->gid << ", getMemberRet: " << getMemberRet;
        res.result(http::status::forbidden);
        marker.setReturnCode(res.result_int());
        return;

    } else if (getMemberRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to query group user info, uid: " << account->uid()
             << ", gid: " << uploadGroupKeysRequest->gid << ", getMemberRet: " << getMemberRet;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }

    // check group version
    Group group;
    auto getGroupRet = m_groups->get(uploadGroupKeysRequest->gid, group);
    if (getGroupRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "onUploadGroupKeysV3, failed to query group, uid: " << account->uid()
             << ", gid: " << uploadGroupKeysRequest->gid << ", getGroupRet: " << getGroupRet;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }
    if (group.version() != static_cast<int32_t>(group::GroupVersion::GroupV3)) {
        LOGE << "onUploadGroupKeysV3, the group is not GroupV3, uid: " << account->uid()
             << ", gid: " << uploadGroupKeysRequest->gid << ", group version: " << group.version();
        res.result(http::status::bad_request);
        marker.setReturnCode(1001);
        return;
    }

    // check mode
    if (uploadGroupKeysRequest->groupKeysMode != GroupKeys::ALL_THE_SAME &&
        uploadGroupKeysRequest->groupKeysMode != GroupKeys::ONE_FOR_EACH) {
        LOGE << "group keys mode value dismatch, mode: " << uploadGroupKeysRequest->groupKeysMode;
        res.result(http::status::bad_request);
        marker.setReturnCode(res.result_int());
        return;
    }

    // insert group keys (CAS)
    auto createTime = nowInMilli();
    GroupKeys groupKeys;
    groupKeys.set_gid(uploadGroupKeysRequest->gid);
    groupKeys.set_version(uploadGroupKeysRequest->version);
    groupKeys.set_groupkeys(nlohmann::json(uploadGroupKeysRequest->groupKeys).dump());
    groupKeys.set_mode(static_cast<GroupKeys::GroupKeysMode>(uploadGroupKeysRequest->groupKeysMode));
    groupKeys.set_creator(account->uid());
    groupKeys.set_createtime(createTime);
    auto insertGroupKeysRet = m_groupKeys->insert(groupKeys);

    if (insertGroupKeysRet == dao::ERRORCODE_SUCCESS) {
        LOGI << "insert group keys success, uid: " << account->uid() << ", gid: " << uploadGroupKeysRequest->gid
             << ", request group keys version: " <<  uploadGroupKeysRequest->version;

        // send switch keys
        sendSwitchGroupKeysWithRetry(account->uid(), uploadGroupKeysRequest->gid, uploadGroupKeysRequest->version);
        res.result(http::status::no_content);
        marker.setReturnCode(res.result_int());

    } else if (insertGroupKeysRet == dao::ERRORCODE_CAS_FAIL) {
        LOGI << "group keys already exists, cas fail, uid: " << account->uid() << ", gid: " << uploadGroupKeysRequest->gid
             << ", request group keys version: " << uploadGroupKeysRequest->version;
        res.result(http::status::conflict);
        marker.setReturnCode(res.result_int());

    } else {
        LOGI << "failed to insert group keys, uid: " << account->uid() << ", gid: " << uploadGroupKeysRequest->gid
             << ", request group keys version: " << uploadGroupKeysRequest->version << ", ret: " << insertGroupKeysRet;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
    }
}

void GroupManagerController::onFetchLatestGroupKeysV3(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onFetchLatestGroupKeysV3");
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto* fetchLatestGroupKeysRequest = boost::any_cast<FetchLatestGroupKeysRequest>(&context.requestEntity);
    LOGI << "onFetchLatestGroupKeysV3, uid: " <<  account->uid() << ", request: " << nlohmann::json(*fetchLatestGroupKeysRequest).dump();

    if (fetchLatestGroupKeysRequest->gids.size() > kMaxGroupBatchFetchLatestKeysV3) {
        LOGE << "the request gids size large than " << kMaxGroupBatchFetchLatestKeysV3 << ", request size are: " << fetchLatestGroupKeysRequest->gids.size();
        res.result(http::status::bad_request);
        marker.setReturnCode(res.result_int());
        return;
    }

    // check whether the user in groups
    std::vector<uint64_t> joinedGids;
    auto getJoinedGroupsRet = m_groupUsers->getJoinedGroups(account->uid(), joinedGids);
    if (getJoinedGroupsRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to query joined groups, uid: " << account->uid() <<  ", getJoinedGroupsRet: " << getJoinedGroupsRet;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }
    std::set<uint64_t> joinedGidSet(joinedGids.begin(), joinedGids.end());
    for (auto& gid : fetchLatestGroupKeysRequest->gids) {
        if (joinedGidSet.find(gid) == joinedGidSet.end()) {
            LOGE << "the uid have not join the group, uid: " << account->uid() << ", gid: " << gid;
            res.result(http::status::forbidden);
            marker.setReturnCode(res.result_int());
            return;
        }
    }

    // fetch group keys
    std::vector<bcm::GroupKeys> groupKeys;
    auto ret = m_groupKeys->getLatestGroupKeys(fetchLatestGroupKeysRequest->gids, groupKeys);
    if (ret == dao::ERRORCODE_NO_SUCH_DATA) {
        LOGI << "onFetchLatestGroupKeysV3, no such data, request: " << nlohmann::json(*fetchLatestGroupKeysRequest).dump() << ", ret: " << ret;
        FetchLatestGroupKeysResponse fetchLatestGroupKeysResponse;
        context.responseEntity = fetchLatestGroupKeysResponse;
        res.result(http::status::ok);
        marker.setReturnCode(res.result_int());
        return;
    } else if (ret != dao::ERRORCODE_SUCCESS) {
        LOGE << "onFetchLatestGroupKeysV3, failed to fetch group keys, uid: " <<  account->uid() << ", request: " << nlohmann::json(*fetchLatestGroupKeysRequest).dump() << ", ret: " << ret;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }

    // response
    auto& device = *(m_accountsManager->getAuthDevice(*account));
    uint32_t deviceId = static_cast<uint32_t>(device.id());

    FetchLatestGroupKeysResponse fetchLatestGroupKeysResponse;
    for (auto& groupKey : groupKeys) {
        // deserialize group keys to object
        JsonSerializerImp<GroupKeysJson> jsonSerializerImp;
        boost::any entity{};
        if (!jsonSerializerImp.deserialize(groupKey.groupkeys(), entity)) {
            LOGE << "failed to deserialize group keys, gid: " << groupKey.gid() << ", version: " << groupKey.version();
            res.result(http::status::internal_server_error);
            marker.setReturnCode(res.result_int());
            return;
        }
        auto* groupKeysJson = boost::any_cast<GroupKeysJson>(&entity);

        FetchLatestGroupKeysResponseKey fetchLatestGroupKeysResponseKey;
        fetchLatestGroupKeysResponseKey.gid = groupKey.gid();
        fetchLatestGroupKeysResponseKey.version = groupKey.version();
        fetchLatestGroupKeysResponseKey.groupKeysMode = static_cast<int32_t>(groupKey.mode());
        fetchLatestGroupKeysResponseKey.encryptVersion = groupKeysJson->encryptVersion;
        // when group mode is normal group
        if (groupKey.mode() == GroupKeys::ALL_THE_SAME) {
            fetchLatestGroupKeysResponseKey.keysV1 = std::move(groupKeysJson->keysV1);
            LOGI << "success get normal group keys, gid: " << fetchLatestGroupKeysResponseKey.gid
                 << ", version: " << fetchLatestGroupKeysResponseKey.version << ", uid: " << account->uid();
        } else if (groupKey.mode() == GroupKeys::ONE_FOR_EACH) {
            bool found = false;
            for (auto& groupKeyEntryV0 : groupKeysJson->keysV0) {
                if (groupKeyEntryV0.uid == account->uid() && groupKeyEntryV0.deviceId == deviceId) {
                    found = true;
                    fetchLatestGroupKeysResponseKey.keysV0 = std::move(groupKeyEntryV0);
                    LOGI << "success get power group keys, gid: " << fetchLatestGroupKeysResponseKey.gid
                         << ", version: " << fetchLatestGroupKeysResponseKey.version << ", uid: " << account->uid();
                    break;
                }
            }

            if (!found) {
                LOGI << "cannot found latest group key for user(gid: " << groupKey.gid() <<
                     ", uid: " << account->uid() <<
                     ", auth deviceID: " << deviceId << ")";
                continue;
            }
        }
        fetchLatestGroupKeysResponse.keys.emplace_back(std::move(fetchLatestGroupKeysResponseKey));
    }

    context.responseEntity = fetchLatestGroupKeysResponse;
    res.result(http::status::ok);
    marker.setReturnCode(res.result_int());
}

void GroupManagerController::onFetchGroupKeysV3(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onFetchGroupKeysV3");
    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto* fetchGroupKeysRequest = boost::any_cast<FetchGroupKeysRequest>(&context.requestEntity);
    LOGI << "onFetchGroupKeysV3, uid: " << account->uid() << ", request: " << nlohmann::json(*fetchGroupKeysRequest).dump();

    if (fetchGroupKeysRequest->versions.size() > kMaxGroupBatchCommonV3) {
        LOGE << "the request version size large than " << kMaxGroupBatchCommonV3 << ", request size are: " << fetchGroupKeysRequest->versions.size();
        res.result(http::status::bad_request);
        marker.setReturnCode(res.result_int());
        return;
    }

    // check whether the user in this group
    GroupUser user;
    auto getMemberRet = m_groupUsers->getMember(fetchGroupKeysRequest->gid, account->uid(), user);
    if (getMemberRet == dao::ERRORCODE_NO_SUCH_DATA) {
        LOGE << "the uid is not group members, uid:" << account->uid() << ", gid:" << fetchGroupKeysRequest->gid << ", getMemberRet " << getMemberRet;
        res.result(http::status::forbidden);
        marker.setReturnCode(res.result_int());
        return;

    } else if (getMemberRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to query group user info, uid:" << account->uid() << ", gid:" << fetchGroupKeysRequest->gid << ", getMemberRet " << getMemberRet;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }

    // fetch group keys
    std::vector<bcm::GroupKeys> groupKeys;
    auto ret = m_groupKeys->get(fetchGroupKeysRequest->gid, fetchGroupKeysRequest->versions, groupKeys);
    if (ret == dao::ERRORCODE_NO_SUCH_DATA) {
        LOGI << "fetch group keys, no such data, request: " << nlohmann::json(*fetchGroupKeysRequest).dump() << ", ret: " << ret;
        FetchGroupKeysResponse fetchGroupKeysResponse;
        fetchGroupKeysResponse.gid = fetchGroupKeysRequest->gid;
        context.responseEntity = fetchGroupKeysResponse;
        res.result(http::status::ok);
        marker.setReturnCode(res.result_int());
        return;
    } else if (ret != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to fetch group keys, request: " << nlohmann::json(*fetchGroupKeysRequest).dump() << ", ret: " << ret;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }

    // response when has version
    auto& device = *(m_accountsManager->getAuthDevice(*account));
    uint32_t deviceId = static_cast<uint32_t>(device.id());

    FetchGroupKeysResponse fetchGroupKeysResponse;
    fetchGroupKeysResponse.gid = fetchGroupKeysRequest->gid;

    for (auto& groupKey : groupKeys) {
        // deserialize group keys to object
        JsonSerializerImp<GroupKeysJson> jsonSerializerImp;
        boost::any entity{};
        if (!jsonSerializerImp.deserialize(groupKey.groupkeys(), entity)) {
            LOGE << "failed to deserialize group keys, gid: " << groupKey.gid() << ", version: " << groupKey.version();
            res.result(http::status::internal_server_error);
            marker.setReturnCode(res.result_int());
            return;
        }
        auto* groupKeysJson = boost::any_cast<GroupKeysJson>(&entity);

        FetchGroupKeysResponseKey fetchGroupKeysResponseKey;
        fetchGroupKeysResponseKey.version = groupKey.version();
        fetchGroupKeysResponseKey.groupKeysMode = static_cast<int32_t>(groupKey.mode());
        fetchGroupKeysResponseKey.encryptVersion = groupKeysJson->encryptVersion;
        // when group mode is normal group
        if (groupKey.mode() == GroupKeys::ALL_THE_SAME) {
            fetchGroupKeysResponseKey.keysV1 = std::move(groupKeysJson->keysV1);
            LOGI << "success get normal group keys, gid: " << fetchGroupKeysRequest->gid
                 << ", version: " << fetchGroupKeysResponseKey.version << ", uid: " << account->uid();
        } else if (groupKey.mode() == GroupKeys::ONE_FOR_EACH) {
            bool found = false;
            for (auto& groupKeyEntryV0 : groupKeysJson->keysV0) {
                if (groupKeyEntryV0.uid == account->uid() && groupKeyEntryV0.deviceId == deviceId) {
                    found = true;
                    fetchGroupKeysResponseKey.keysV0 = std::move(groupKeyEntryV0);
                    LOGI << "success get power group keys, gid: " << fetchGroupKeysRequest->gid
                         << ", version: " << fetchGroupKeysResponseKey.version << ", uid: " << account->uid();
                    break;
                }
            }

            if (!found) {
                LOGI << "cannot found group key for user(gid: " << fetchGroupKeysRequest->gid <<
                     ", uid: " << account->uid() << ", version: " << groupKey.version() <<
                     ", auth deviceID: " << deviceId << ")";
                continue;
            }
        }
        fetchGroupKeysResponse.keys.emplace_back(std::move(fetchGroupKeysResponseKey));
    }

    context.responseEntity = fetchGroupKeysResponse;
    res.result(http::status::ok);
    marker.setReturnCode(res.result_int());
}


void GroupManagerController::fireGroupKeysUpdateV3(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "fireGroupKeysUpdateV3");

    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto* fireGroupKeysUpdateRequest = boost::any_cast<FireGroupKeysUpdateRequest>(&context.requestEntity);
    LOGI << "fireGroupKeysUpdateV3, uid: " << account->uid() << ", request: " << nlohmann::json(*fireGroupKeysUpdateRequest).dump();

    if (fireGroupKeysUpdateRequest->gids.size() > kMaxGroupBatchCommonV3) {
        LOGE << "the request gids size large than " << kMaxGroupBatchCommonV3 << ", request size are: " << fireGroupKeysUpdateRequest->gids.size();
        res.result(http::status::bad_request);
        marker.setReturnCode(res.result_int());
        return;
    }

    // check throttle
    for (const auto& gid : fireGroupKeysUpdateRequest->gids) {
        if (LimitLevel::LIMITED == m_groupFireKeysUpdateLimiter->acquireAccess(account->uid() + "_" + std::to_string(gid))) {
            LOGE << "the request has been rejected, uid: " << account->uid() << "; gid: " << gid;
            res.result(static_cast<unsigned>(bcm::custom_http_status::limiter_rejected));
            marker.setReturnCode(res.result_int());
            return;
        }
    }

    // check whether the user in this group
    std::vector<uint64_t> joinedGids;
    auto getJoinedGroupsRet = m_groupUsers->getJoinedGroups(account->uid(), joinedGids);
    if (getJoinedGroupsRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to query joined groups, uid: " << account->uid() <<  ", getJoinedGroupsRet: " << getJoinedGroupsRet;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }
    std::set<uint64_t> joinedGidSet(joinedGids.begin(), joinedGids.end());
    for (auto& gid : fireGroupKeysUpdateRequest->gids) {
        if (joinedGidSet.find(gid) == joinedGidSet.end()) {
            LOGE << "the uid have not join the group, uid: " << account->uid() << ", gid: " << gid;
            res.result(http::status::forbidden);
            marker.setReturnCode(res.result_int());
            return;
        }
    }

    FireGroupKeysUpdateResponse fireGroupKeysUpdateResponse;
    for (auto& gid : fireGroupKeysUpdateRequest->gids) {
        dao::GroupCounter counter;
        auto counterRet = m_groupUsers->queryGroupMemberInfoByGid(gid, counter);
        if (counterRet != dao::ERRORCODE_SUCCESS) {
            LOGE << "failed to queryGroupMemberInfoByGid, gid: " << gid << ", uid:" << account->uid() << ", ret: " << counterRet;
            res.result(http::status::internal_server_error);
            marker.setReturnCode(res.result_int());
            return;
        }

        if (sendGroupKeysUpdateRequestWhenMemberChanges(account->uid(), gid, counter.memberCnt)) {
            fireGroupKeysUpdateResponse.success.emplace_back(gid);
        } else {
            fireGroupKeysUpdateResponse.fail.emplace_back(gid);
        }
    }

    context.responseEntity = fireGroupKeysUpdateResponse;
    res.result(http::status::ok);
    marker.setReturnCode(res.result_int());
}

void GroupManagerController::onKickGroupMemberV3(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onKickGroupMemberV3");

    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& statics = context.statics;
    statics.setUid(account->uid());

    auto* kickGroupMemberBody = boost::any_cast<KickGroupMemberBody>(&context.requestEntity);
    uint64_t gid = kickGroupMemberBody->gid;
    LOGI << "onKickGroupMemberV3, uid: " << account->uid() << ", request: " << nlohmann::json(*kickGroupMemberBody).dump();

    GroupResponse response;
    bool checked = kickGroupMemberBody->check(response, account->uid());
    if (!checked) {
        LOGE << "onKickGroupMemberV3, parameter error, code: " << response.code << ", " << response.msg;
        res.result(http::status::bad_request);
        marker.setReturnCode(res.result_int());
        return;
    }

    // check group version
    Group group;
    auto getGroupRet = m_groups->get(gid, group);
    if (getGroupRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "onKickGroupMemberV3, failed to query group, uid: " << account->uid()
             << ", gid: " << gid << ", getGroupRet: " << getGroupRet;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }
    if (group.version() != static_cast<int32_t>(group::GroupVersion::GroupV3)) {
        LOGE << "onKickGroupMemberV3, the group is not GroupV3, uid: " << account->uid()
             << ", gid: " << gid << ", group version: " << group.version();
        res.result(http::status::bad_request);
        marker.setReturnCode(1001);
        return;
    }

    // ADD V3
    dao::GroupCounter counter;
    auto counterRet = m_groupUsers->queryGroupMemberInfoByGid(gid, counter);
    if (counterRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to queryGroupMemberInfoByGid, gid: " << gid << ", uid:" << account->uid() << ", ret: " << counterRet;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }
    // ADD V3 End

    GroupUser::Role role;
    auto roleRet = m_groupUsers->getMemberRole(gid, account->uid(), role);
    LOGD << "get member role: " << account->uid() << ": " << gid << ": " << roleRet << ": " << role;

    if (roleRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to getMemberRole, gid: " << gid << ", uid:" << account->uid() << ", ret: " << roleRet;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }

    if (role != GroupUser::ROLE_OWNER) {
        LOGE << "kicker is not owner, gid: " << gid << ", uid:" << account->uid();
        res.result(http::status::bad_request);
        marker.setReturnCode(res.result_int());
        return ;
    }

    std::vector<GroupUser> users;
    auto queryRet = m_groupUsers->getMemberBatch(gid, kickGroupMemberBody->members, users);

    if (queryRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to getMemberBatch, gid: " << gid << ", ret: " << queryRet;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }

    std::map<std::string, GroupUser> memberInfo;
    std::vector<std::string> toKickMember;

    for (const auto& m: users) {
        memberInfo[m.uid()] =  m;
    }

    GroupSysMsgBody msg;
    msg.action = QUIT_GROUP;

    for (const auto& m : kickGroupMemberBody->members) {
        if (memberInfo.find(m) != memberInfo.end()) {
            toKickMember.push_back(m);
            msg.members.emplace_back(memberInfo[m].uid(), memberInfo[m].uid(), memberInfo[m].role());
        }
    }

    auto kickRet = m_groupUsers->delMemberBatch(gid, toKickMember);

    if (kickRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to delMemberBatch, gid: " << gid << ", ret: " << queryRet;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }

    pubGroupSystemMessage(account->uid(), gid, GROUP_MEMBER_UPDATE, nlohmann::json(msg).dump());

    for (const auto& m : toKickMember) {
        publishGroupUserEvent(gid, m, INTERNAL_USER_QUIT_GROUP);
    }

    // ADD V3, send GROUP_UPDATE_GROUP_KEYS_REQUEST
    uint32_t groupMembersAfterLeave = counter.memberCnt >= kickGroupMemberBody->members.size() ?
                                      counter.memberCnt - kickGroupMemberBody->members.size() : 0;
    sendGroupKeysUpdateRequestWhenMemberChanges(account->uid(), gid, groupMembersAfterLeave);
    // ADD V3 END

    res.result(http::status::no_content);
    marker.setReturnCode(res.result_int());
}

void GroupManagerController::onLeaveGroupV3(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onLeaveGroupV3");

    auto& res = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& statics = context.statics;
    statics.setUid(account->uid());

    auto* leaveGroupBody = boost::any_cast<LeaveGroupBody>(&context.requestEntity);
    uint64_t gid = leaveGroupBody->gid;
    LOGI << "onLeaveGroupV3, gid: " << gid << ", uid: " << account->uid();

    GroupResponse response;
    bool checked = leaveGroupBody->check(response);
    if (!checked) {
        LOGE << "onLeaveGroupV3, parameter error, code: " << response.code << ", " << response.msg;
        res.result(http::status::bad_request);
        marker.setReturnCode(res.result_int());
        return;
    }

    // check group version
    Group group;
    auto getGroupRet = m_groups->get(gid, group);
    if (getGroupRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "onLeaveGroupV3, failed to query group, uid: " << account->uid()
             << ", gid: " << gid << ", getGroupRet: " << getGroupRet;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }
    if (group.version() != static_cast<int32_t>(group::GroupVersion::GroupV3)) {
        LOGE << "onLeaveGroupV3, the group is not GroupV3, uid: " << account->uid()
             << ", gid: " << gid << ", group version: " << group.version();
        res.result(http::status::bad_request);
        marker.setReturnCode(1001);
        return;
    }

    auto role = GroupUser::ROLE_UNDEFINE;
    auto nextOwnerRole = GroupUser::ROLE_UNDEFINE;
    dao::GroupCounter counter;
    auto roleRet = m_groupUsers->queryGroupMemberInfoByGid(gid, counter, account->uid(), role, leaveGroupBody->nextOwner, nextOwnerRole);

    if (roleRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to queryGroupMemberInfoByGid, gid: " << gid << ", uid:" << account->uid() << ", ret: " << roleRet;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }

    if (role != GroupUser::ROLE_OWNER && role != GroupUser::ROLE_MEMBER) {
        LOGE << "the user is not the group member, gid:" << gid << ", uid:" << account->uid();
        res.result(http::status::bad_request);
        marker.setReturnCode(res.result_int());
        return;
    }

    if (counter.memberCnt == 0) {
        LOGE << "the group member is 0, cannot leave group, gid:" << gid << ", uid:" << account->uid();
        res.result(http::status::bad_request);
        marker.setReturnCode(res.result_int());
        return;
    }

    bool needNextOwner = false;
    if (role == GroupUser::ROLE_OWNER && counter.memberCnt > 1) {
        needNextOwner = true;
    }

    if (needNextOwner) {
        const auto& nextOwner = leaveGroupBody->nextOwner;
        if (nextOwner.empty() || nextOwner == account->uid() ||
            (nextOwnerRole != GroupUser::ROLE_OWNER && nextOwnerRole != GroupUser::ROLE_MEMBER)) {
            LOGE << "the next wanted owner cannot become owner, gid:" << gid << ", uid:" << account->uid() << ", nextOwner:" << nextOwner;
            res.result(http::status::bad_request);
            marker.setReturnCode(res.result_int());
            return;
        }
    }

    if (needNextOwner) {
        auto upRet = m_groupUsers->update(gid, leaveGroupBody->nextOwner, nlohmann::json{{"role", static_cast<int32_t>(GroupUser::ROLE_OWNER)}});
        LOGD << "group user update V3: " << gid << ": " << leaveGroupBody->nextOwner << ": " << upRet;
        if (upRet != dao::ERRORCODE_SUCCESS) {
            LOGE << "failed to update next owner, gid: " << gid << ", uid:" << account->uid() << ", ret: " << upRet << ", next owner:" << leaveGroupBody->nextOwner;
            res.result(http::status::internal_server_error);
            marker.setReturnCode(res.result_int());
            return;
        }
    }

    auto delRet = m_groupUsers->delMember(gid, account->uid());

    if (delRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to delete member, gid: " << gid << ", uid:" << account->uid() << ", ret: " << delRet;
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
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

    // ADD V3, send GROUP_UPDATE_GROUP_KEYS_REQUEST
    uint32_t groupMembersAfterLeave = counter.memberCnt >= 1 ? counter.memberCnt - 1 : 0;
    sendGroupKeysUpdateRequestWhenMemberChanges(account->uid(), gid, groupMembersAfterLeave);
    // ADD V3 END


    res.result(http::status::no_content);
    marker.setReturnCode(res.result_int());
}

void GroupManagerController::onCreateGroupV3(HttpContext &context)
{
    LOGI << "onCreateGroupV3 start ...";
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onCreateGroupV3");

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
                LOGW << "onCreateGroupV3: "<< "dont support multi device: " << account->uid();
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
    auto *createGroupBody = boost::any_cast<CreateGroupBodyV3>(&context.requestEntity);
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

    Group::EncryptStatus encryptFlag = Group::ENCRYPT_STATUS_ON;
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
    // V3 ADD
    group.set_encryptedgroupinfosecret(createGroupBody->encryptedGroupInfoSecret);
    group.set_version(static_cast<int32_t>(group::GroupVersion::GroupV3));
    group.set_encryptedephemeralkey(createGroupBody->encryptedEphemeralKey);

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

    // insert group keys before insert into users
    // make sure that when group keys insert fail, the group have not created success!
    GroupKeys groupKeys;
    groupKeys.set_gid(gid);
    // first version is 0
    groupKeys.set_version(0);
    groupKeys.set_groupkeys(nlohmann::json(createGroupBody->groupKeys).dump());
    groupKeys.set_mode(static_cast<GroupKeys::GroupKeysMode>(createGroupBody->groupKeysMode));
    groupKeys.set_creator(account->uid());
    groupKeys.set_createtime(createTime);
    auto insertGroupKeysRet = m_groupKeys->insert(groupKeys);
    if (insertGroupKeysRet != dao::ERRORCODE_SUCCESS) {
        m_groups->del(gid);
        LOGE << "failed to insert group keys into group, gid: " << group.gid();
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }
    LOGI << "insert group keys success, uid: " << account->uid() << ", gid: " << gid << ", version: 0, group keys:"
         << nlohmann::json(createGroupBody->groupKeys).dump();

    // put all users into this group
    std::vector<GroupUser> users;
    const auto &members = createGroupBody->members;
    const auto &proofs = createGroupBody->memberProofs;
    const auto &secret_keys = createGroupBody->membersGroupInfoSecrets;
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
        user.set_groupinfosecret(secret_keys[i]);
        // ADD V3
        user.set_proof(proofs[i]);

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
    owner.set_groupinfosecret(createGroupBody->ownerSecretKey);
    // ADD V3
    owner.set_proof(createGroupBody->ownerProof);
    users.push_back(std::move(owner));
    auto insertMembersRet = m_groupUsers->insertBatch(users);
    if (insertMembersRet != dao::ERRORCODE_SUCCESS) {
        m_groups->del(gid);
        m_groupKeys->clear(gid);
        LOGE << "failed to insert user into group, gid: " << group.gid();
        res.result(http::status::internal_server_error);
        marker.setReturnCode(res.result_int());
        return;
    }

    LOGI << "insert group members success: " << account->uid() << ": " << gid << ": "
         << nlohmann::json(createGroupBody->members).dump();

    GroupSysMsgBody sysMsgBody;
    sysMsgBody.action = ENTER_GROUP;

    for (const auto &m : users) {
        publishGroupUserEvent(gid, m.uid(), INTERNAL_USER_ENTER_GROUP);
        if (m.uid() != account->uid()) {
            sysMsgBody.members.emplace_back(SimpleGroupMemberInfo{m.uid(), m.nick(), m.role()});
        }
    }

    pubGroupSystemMessage(account->uid(), gid, GroupMsg::TYPE_MEMBER_UPDATE, nlohmann::json(sysMsgBody).dump());
    // pub GROUP_SWITCH_GROUP_KEYS, version is 0;
    sendSwitchGroupKeysWithRetry(account->uid(), gid, 0);

    CreateGroupBodyV3Resp createGroupBodyV3Resp;
    createGroupBodyV3Resp.gid = gid;
    context.responseEntity = createGroupBodyV3Resp;
    res.result(http::status::ok);
    marker.setReturnCode(res.result_int());
    LOGI << "onCreateGroupV3, finish create group, status: 200, gid: " << gid << ", creator: " << account->uid();
}

void GroupManagerController::onDhKeysV3(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onDhKeysV3");

    DhKeysRequest* request = boost::any_cast<DhKeysRequest>(&context.requestEntity);
    auto& response = context.response;
    Account* account = boost::any_cast<Account>(&context.authResult);

    if (!request->check()) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }

    if (LimitLevel::LIMITED == m_dhKeysLimiter->acquireAccess(account->uid())) {
        LOGE << "the request has been rejected";
        response.result(static_cast<unsigned>(bcm::custom_http_status::limiter_rejected));
        marker.setReturnCode(response.result_int());
        return;
    }

    std::vector<bcm::Keys> keys;
    auto ec = m_accountsManager->getKeys(request->uids, keys);
    if (dao::ERRORCODE_NO_SUCH_DATA != ec && dao::ERRORCODE_SUCCESS != ec) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, "database error when getting members for this group");
#endif
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        std::ostringstream oss;
        std::string sep("");
        for (const auto& u : request->uids) {
            oss << sep << u;
            sep = ",";
        }
        LOGE << "failed to get keys, uids:" << oss.str() << ", error:" << ec;
        return;
    }
    
    DhKeysResponse respEntity;
    respEntity.keys = keys;
    context.responseEntity = respEntity;
    response.result(http::status::ok);
    marker.setReturnCode(response.result_int());

    //LOGT << "success to query dh keys" << context.responseEntity << ")";
}

void GroupManagerController::onPrepareKeyUpdateV3(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onPrepareKeyUpdateV3");

    PrepareKeyUpdateRequestV3* request = boost::any_cast<PrepareKeyUpdateRequestV3>(&context.requestEntity);
    Account* account = boost::any_cast<Account>(&context.authResult);
    auto& response = context.response;
    
    LOGI << "uid: " << account->uid() << "request: " << nlohmann::json(*request).dump();

    bcm::Group group;
    dao::ErrorCode ec = m_groups->get(request->gid, group);
    if (ec == dao::ERRORCODE_NO_SUCH_DATA) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif
        LOGE << "group not existed, gid: " << request->gid << ", error: " << ec;
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }
    if (ec != dao::ERRORCODE_SUCCESS) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif        
        LOGE << "failed to get group, gid: " << request->gid << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }

    if (!checkGroupVersion(group, group::GroupVersion::GroupV3)) {
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }

    // 1. check the group key version is more than the latest one in db
    bcm::dao::rpc::LatestModeAndVersion mv;
    ec = m_groupKeys->getLatestModeAndVersion(request->gid, mv);
    if (ec != dao::ERRORCODE_SUCCESS && ec != dao::ERRORCODE_NO_SUCH_DATA) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif
        LOGE << "getLatestModeAndVersion failed, gid: " << request->gid 
             << ", uid: " << account->uid() << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;        
    }
    if (ec == dao::ERRORCODE_SUCCESS && request->version <= mv.version()) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif
        LOGT << "version conflict, gid: " << request->gid << ", uid: " << account->uid() 
             << "latest version: " << mv.version() << ", request version: " << request->version;
        response.result(http::status::conflict);
        marker.setReturnCode(response.result_int());
        return;
    }

    PrepareKeyUpdateResponseV3 resp;
    resp.keys.clear();

    // select top 5 online users as candidates and then get the signed prekey and onetime key
    OnlineMsgMemberMgr::UserSet candidates;
    selectKeyDistributionCandidates(request->gid, request->version, m_groupConfig.keySwitchCandidateCount, candidates);
    if (candidates.find(DispatchAddress(account->uid(), account->authdeviceid())) == candidates.end()) {
        context.responseEntity = resp;
        response.result(http::status::conflict);
        marker.setReturnCode(response.result_int());
        return;
    }

    if (static_cast<GroupKeys::GroupKeysMode>(request->mode) == GroupKeys::ONE_FOR_EACH) {
        std::vector<bcm::Keys> keys;
        // query cache first
        if (!m_keysCache.get(request->gid, request->version, keys)) {
            auto ec = m_accountsManager->getKeysByGid(request->gid, keys);
            if (dao::ERRORCODE_NO_SUCH_DATA != ec && dao::ERRORCODE_SUCCESS != ec) {
#ifndef NDEBUG
                //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, "database error when getting members for this group");
#endif
                response.result(http::status::internal_server_error);
                marker.setReturnCode(response.result_int());
                LOGE << "failed to get keys, gid: " << request->gid << ", uid: " << account->uid() << ", error: " << ec;
                return;
            }
            m_keysCache.set(request->gid, request->version, keys);
        }
        resp.keys = keys;
    }
    context.responseEntity = resp;
    response.result(http::status::ok);
    marker.setReturnCode(response.result_int());

    LOGT << "success to query dh keys, gid: " << request->gid << ", uid: " << account->uid() << ", keys size: " << resp.keys.size() << "";
}

void GroupManagerController::onGroupJoinRequstV3(HttpContext& context)
{
    LOGT << "onGroupJoinRequstV3 start...";
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onGroupJoinRequstV3");

    auto& response = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);

    GroupJoinRequestV3* req = boost::any_cast<GroupJoinRequestV3>(&context.requestEntity);
    if (!req->check()) {
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }

    std::string limiterId = getMemberJoinLimiterId(req->gid, account->uid());
    if (m_groupMemberJoinLimiter->limited(limiterId) == LimitLevel::LIMITED) {
        LOGE << "the request has been rejected, gid: " << req->gid << ", uid: " << account->uid();
        response.result(static_cast<unsigned>(bcm::custom_http_status::limiter_rejected));
        marker.setReturnCode(response.result_int());
        return;
    }

    bcm::Group group;
    dao::ErrorCode ec = m_groups->get(req->gid, group);
    if (ec == dao::ERRORCODE_NO_SUCH_DATA) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif
        LOGE << "group not existed, gid: " << req->gid << ", error: " << ec;
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }
    if (ec != dao::ERRORCODE_SUCCESS) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif        
        LOGE << "failed to get group, gid: " << req->gid << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }

    if (!checkGroupVersion(group, group::GroupVersion::GroupV3)) {
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }

    GroupJoinResponseV3 respEntity;
    bcm::GroupUser user;
    ec = m_groupUsers->getMember(req->gid, account->uid(), user);
    if (ec == dao::ERRORCODE_SUCCESS) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif
        LOGI << "user already existed" << req->gid << ", uid: " << account->uid() << ", error: " << ec;
        respEntity.ownerConfirm = !!group.ownerconfirm();
        respEntity.encryptedGroupInfoSecret = group.encryptedgroupinfosecret();
        context.responseEntity = respEntity;
        response.result(http::status::ok);
        marker.setReturnCode(response.result_int());
        return;
    }

    bcm::Account owner;
    if (!getGroupOwner(group.gid(), owner)) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif
        LOGE << "failed to get owner, gid: " << group.gid();
        marker.setReturnCode(group::ERRORCODE_INTERNAL_ERROR);
        response.result(http::status::internal_server_error);
        return;
    }
    // check the data consistency to make sure the settings in database are correct
    if (group.shareqrcodesetting().empty()
            || group.sharesignature().empty()
            || group.shareandownerconfirmsignature().empty()) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif
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
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif
        LOGE << "group setting verify failed, gid: " << req->gid << ", owner confirm: " << group.ownerconfirm()
             << ", setting: " << group.shareqrcodesetting() << ", signature: " << group.shareandownerconfirmsignature();
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }
    // check the qr code is correct
    if (req->qrCodeToken != group.sharesignature()) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif        
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
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif        
        LOGE << "invalid signature, gid: " << req->gid << ", code: " << req->qrCode
             << ", qr code token: " << req->qrCodeToken << ", signature: " << req->signature;
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }

    if (!group.ownerconfirm()) {
        // return encrypted_group_info_secret to user
        bcm::QrCodeGroupUser user;
        user.set_gid(req->gid);
        user.set_uid(account->uid());
        ec = m_qrCodeGroupUsers->set(user, kQrCodeGroupUserTtl);
        if (ec != dao::ERRORCODE_SUCCESS) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif            
            LOGE << "failed to query qr code group user, error: " << ec;
            response.result(http::status::internal_server_error);
            marker.setReturnCode(response.result_int());
            return;
        }
        respEntity.ownerConfirm = false;
        respEntity.encryptedGroupInfoSecret = group.encryptedgroupinfosecret();

        context.responseEntity = respEntity;
        response.result(http::status::ok);
        marker.setReturnCode(response.result_int());
        return;
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
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif
        LOGE << "failed to insert into pending group user list, gid" << pgu.gid() << ", uid: " << pgu.uid()
             << ", signature: " << pgu.signature() << ", comment: " << pgu.comment() << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }
    context.responseEntity = respEntity;
    response.result(http::status::ok);
    marker.setReturnCode(response.result_int());
    // pub the message to owner
    pubReviewJoinRequestToOwner(req->gid, owner.uid());
}

void GroupManagerController::onAddMeRequstV3(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onAddMeRequstV3");

    AddMeRequestV3* request = boost::any_cast<AddMeRequestV3>(&context.requestEntity);
    Account* account = boost::any_cast<Account>(&context.authResult);
    auto& response = context.response;

    if (!request->check()) {
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }

    std::string limiterId = getMemberJoinLimiterId(request->gid, account->uid());
    if (m_groupMemberJoinLimiter->acquireAccess(limiterId) == LimitLevel::LIMITED) {
        LOGE << "the request has been rejected, gid: " << request->gid << ", uid: " << account->uid();
        response.result(static_cast<unsigned>(bcm::custom_http_status::limiter_rejected));
        marker.setReturnCode(response.result_int());
        return;
    }

    // 1. check whether the group exists
    Group group;
    auto ec = m_groups->get(request->gid, group);
    if (ec != dao::ERRORCODE_SUCCESS) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif
        LOGE << "failed to get group, gid" << request->gid << ", error: " << ec;
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }
    
    if (!checkGroupVersion(group, group::GroupVersion::GroupV3)) {
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }

    // 2. check whether the user is already in the group
    bcm::GroupUser user;
    ec = m_groupUsers->getMember(request->gid, account->uid(), user);
    if (ec == dao::ERRORCODE_SUCCESS) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif
        LOGI << "user already existed, gid: " << request->gid << ", uid: " << account->uid() << ", error: " << ec;
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, "success");
        response.result(http::status::ok);
        marker.setReturnCode(response.result_int());
        return;
    }

    // 3. check if there is a qr code group user for this user
    bcm::QrCodeGroupUser qrCodeGroupUser;
    ec = m_qrCodeGroupUsers->get(request->gid, account->uid(), qrCodeGroupUser);
    if (ec != dao::ERRORCODE_SUCCESS) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif
        LOGE << "failed to query qr code group user for gid: " << request->gid
             << ", uid: " << account->uid();
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;        
    }

    // 4. get members count
    dao::GroupCounter counter;
    ec = m_groupUsers->queryGroupMemberInfoByGid(request->gid, counter);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to queryGroupMemberInfoByGid, gid: " 
             << request->gid << ", uid:" << account->uid() << ", error: " << ec;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }

    // 5. insert into group_users_pubkey
    auto now = nowInMilli();
    user.set_uid(account->uid());
    user.set_nick(account->uid());
    user.set_role(GroupUser::ROLE_MEMBER);
    user.set_gid(request->gid);
    user.set_lastackmid(group.lastmid());
    user.set_status(GroupUser::STATUS_DEFAULT);
    user.set_updatetime(now);
    user.set_createtime(now);
    user.set_encryptedkey("");
    user.set_groupinfosecret(request->groupInfoSecret);
    user.set_proof(request->proof);
    ec = m_groupUsers->insert(user);
    if (ec != dao::ERRORCODE_SUCCESS) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif
        LOGE << "failed to insert group user for gid: " << user.gid()
             << ", uid: " << user.uid();
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;        
    }

    // 6. send events
    publishGroupUserEvent(request->gid, account->uid(), INTERNAL_USER_ENTER_GROUP);
    SimpleGroupMemberInfo info(user.uid(), user.nick(), user.role());
    GroupSysMsgBody msg{ENTER_GROUP, {info}};
    pubGroupSystemMessage(account->uid(), request->gid, GROUP_MEMBER_UPDATE, nlohmann::json(msg).dump());
    sendGroupKeysUpdateRequestWhenMemberChanges(account->uid(), request->gid, counter.memberCnt + 1);
    response.result(http::status::ok);
    marker.setReturnCode(response.result_int());
}

void GroupManagerController::onInviteGroupMemberV3(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onInviteGroupMemberV3");
    auto &response = context.response;

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
                LOGW << "onInviteGroupMemberV3: "<< "dont support multi device: " << account->uid();
                response.result(static_cast<unsigned>(custom_http_status::upgrade_requried));
                return;
            }
        }
    }

    InviteGroupMemberRequestV3* request = boost::any_cast<InviteGroupMemberRequestV3>(&context.requestEntity);
    uint64_t gid = request->gid;

    if (!request->check()) {
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }

    for (const auto& item : request->members) {
        if (m_groupMemberJoinLimiter->acquireAccess(getMemberJoinLimiterId(gid, item)) == LimitLevel::LIMITED) {
            LOGE << "the request has been rejected, gid: " << gid << ", uid: " << item;
            response.result(static_cast<unsigned>(bcm::custom_http_status::limiter_rejected));
            marker.setReturnCode(response.result_int());
            return;
        }
    }

    if (account->uid() != "bcm_backend_manager") {
        GroupUser::Role role;
        auto roleRet = m_groupUsers->getMemberRole(gid, account->uid(), role);
        LOGD << "get member role: " << account->uid() << ": " << gid << ": " << roleRet << ": " << role;
        if (roleRet != dao::ERRORCODE_SUCCESS) {
            LOGE << "failed to get role, gid: " << gid << ", uid:" << account->uid() << ", error:" << roleRet;
            if (roleRet == dao::ERRORCODE_NO_SUCH_DATA) {
                response.result(http::status::not_found);
            } else {
                response.result(http::status::internal_server_error);
            }
            marker.setReturnCode(response.result_int());
            return;
        }
        if (role != GroupUser::ROLE_OWNER && role != GroupUser::ROLE_MEMBER && role != GroupUser::ROLE_ADMINISTROR) {
            LOGE << "insufficient permission, gid: " << gid << ", uid:" << account->uid() << ", role:" << role;
            response.result(http::status::forbidden);
            marker.setReturnCode(response.result_int());
            return;
        }

        const std::vector<std::string>& members = request->members;
        if (!checkBidirectionalRelationshipBypassSubscribers(account, gid, members, response)) {
            marker.setReturnCode(response.result_int());
            return;
        }
    }

    Group group;
    auto queryRet = m_groups->get(gid, group);
    if (queryRet != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to get group, code: " << queryRet;
        if (queryRet == dao::ERRORCODE_NO_SUCH_DATA) {
            response.result(http::status::not_found);
        } else {
            response.result(http::status::internal_server_error);
        }
        marker.setReturnCode(response.result_int());
        return;
    }

    if (!checkGroupVersion(group, group::GroupVersion::GroupV3)) {
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }

    std::string ownerUid;
    dao::ErrorCode ec = m_groupUsers->getGroupOwner(gid, ownerUid);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGE << "group owner data corrupted, gid: " << gid << ", error: " << ec;
        marker.setReturnCode(group::ERRORCODE_INTERNAL_ERROR);
        response.result(http::status::internal_server_error);
        return;
    }

    int32_t ownerConfirm = group.ownerconfirm();
    if (!!ownerConfirm && ownerUid != account->uid()) {
        // check data, signature
        if (request->members.size() != request->signatureInfos.size()) {
            LOGE << "invalid signature, member size: " << request->members.size()
                 << ", signature size: " << request->signatureInfos.size();
            response.result(http::status::bad_request);
            marker.setReturnCode(response.result_int());
            return;
        }
        for (const auto& item : request->signatureInfos) {
            std::string decoded = Base64::decode(item);
            nlohmann::json j = nlohmann::json::parse(decoded);
            decoded = Base64::decode(j.at("data").get<std::string>());
            if (!AccountHelper::verifySignature(account->publickey(), decoded, j.at("sig").get<std::string>())) {
                LOGE << "invalid signature: " << item;
                response.result(http::status::bad_request);
                marker.setReturnCode(response.result_int());
                return;
            }
        }
    } else {
        if (request->members.size() != request->memberProofs.size() ||
            request->members.size() != request->memberGroupInfoSecrets.size()) {
            LOGE << "members, memberProofs, memberGroupInfoSecrets size mismatch, size: "
                 << request->members.size() << ", " << request->memberProofs.size()
                 << ", " << request->memberGroupInfoSecrets.size();
            response.result(http::status::bad_request);
            marker.setReturnCode(response.result_int());
            return;
        }
    }

    auto createTime = nowInMilli();
    auto updateTime = createTime;

    std::map<std::string, bcm::GroupUser::Role> roleMap;
    for (const auto &m : request->members) {
        roleMap[m] = GroupUser::ROLE_UNDEFINE;
    }
    auto roleMapRet = m_groupUsers->getMemberRoles(gid, roleMap);
    if (roleMapRet != dao::ERRORCODE_SUCCESS
            && roleMapRet != dao::ERRORCODE_NO_SUCH_DATA) {
        LOGE << "failed to get member role, code: " << roleMapRet;
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        return;
    }

    if (!!ownerConfirm && ownerUid != account->uid()) {//insert into pending_group_user
        std::vector<PendingGroupUser> pendingUsers;
        const auto &members    = request->members;
        const auto &signatures = request->signatureInfos;
        InviteGroupMemberResponseV3 result;

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
                response.result(http::status::internal_server_error);
                marker.setReturnCode(response.result_int());
                return;
            }
            result.successMembers.emplace_back(pendingUser.uid(), pendingUser.uid(), GroupUser::ROLE_MEMBER);
        }

        context.responseEntity = result;
        response.result(http::status::ok);
        marker.setReturnCode(response.result_int());

        pubReviewJoinRequestToOwner(group.gid(), ownerUid);
        return;
    } else {
        //insert into group_user table,
        std::vector<GroupUser> users;
        InviteGroupMemberResponseV3 result;
        const auto& members = request->members;
        const auto& proofs = request->memberProofs;
        const auto& groupInfoSecrets = request->memberGroupInfoSecrets;
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
                    user.set_groupinfosecret(groupInfoSecrets[i]);
                    user.set_proof(proofs[i]);
                    users.push_back(std::move(user));
                }
            } else {
                result.failedMembers.push_back(members[i]);
            }
        }

        // all users are already in this group
        if (users.empty()) {
            context.responseEntity = result;
            response.result(http::status::ok);
            marker.setReturnCode(response.result_int());
            return;
        }

        // get member count
        dao::GroupCounter counter;
        ec = m_groupUsers->queryGroupMemberInfoByGid(request->gid, counter);
        if (ec != dao::ERRORCODE_SUCCESS) {
            LOGE << "failed to queryGroupMemberInfoByGid, gid: " 
                << request->gid << ", uid:" << account->uid() << ", error: " << ec;
            response.result(http::status::internal_server_error);
            marker.setReturnCode(response.result_int());
            return;
        }

        auto insertRet = m_groupUsers->insertBatch(users);
        if (insertRet != dao::ERRORCODE_SUCCESS) {
            LOGE << "failed to insert pending group user, error: " << insertRet;
            response.result(http::status::internal_server_error);
            marker.setReturnCode(response.result_int());
            return;
        }

        for (const auto &m : users) {
            result.successMembers.emplace_back(m.uid(), m.uid(), m.role());
            publishGroupUserEvent(gid, m.uid(), INTERNAL_USER_ENTER_GROUP);
        }

        context.responseEntity = result;
        response.result(http::status::ok);
        marker.setReturnCode(response.result_int());

        GroupSysMsgBody msg{ENTER_GROUP, result.successMembers};
        pubGroupSystemMessage(account->uid(), gid, GROUP_MEMBER_UPDATE, nlohmann::json(msg).dump());
        sendGroupKeysUpdateRequestWhenMemberChanges(account->uid(), request->gid, counter.memberCnt + users.size());
    }
}

void GroupManagerController::onReviewJoinRequestV3(bcm::HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onReviewJoinRequestV3");

    auto& response = context.response;
    auto* account = boost::any_cast<Account>(&context.authResult);
    ReviewJoinResultRequestV3* req = boost::any_cast<ReviewJoinResultRequestV3>(&context.requestEntity);

    if (!req->check()) {
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

    if (!checkGroupVersion(group, group::GroupVersion::GroupV3)) {
        response.result(http::status::bad_request);
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

    // load the user role if the user is already in this group
    std::set<std::string> delUsers;
    std::vector<std::string> uids;
    std::vector<GroupUser> users;
    std::vector<ReviewJoinResultV3> acceptedList;
    for (const auto& item : req->list) {
        delUsers.emplace(item.uid);
        if (!item.accepted) {
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
    std::set<std::string> existedUsers;
    for (const auto& u : users) {
        existedUsers.emplace(u.uid());
    }

    std::vector<bcm::GroupUser> insertList;
    int64_t nowMs = nowInMilli();
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
            user.set_proof(item.proof);
            user.set_groupinfosecret(item.groupInfoSecret);
            insertList.emplace_back(std::move(user));
            invitees[item.inviter].emplace(item.uid);
            //continue;
        }
    }

    // insert the accepted users into this group
    if (!insertList.empty()) {
        dao::GroupCounter counter;
        ec = m_groupUsers->queryGroupMemberInfoByGid(req->gid, counter);
        if (ec != dao::ERRORCODE_SUCCESS) {
            LOGE << "failed to queryGroupMemberInfoByGid, gid: " 
                << req->gid << ", uid:" << account->uid() << ", error: " << ec;
            response.result(http::status::internal_server_error);
            marker.setReturnCode(response.result_int());
            return;
        }

        ec = m_groupUsers->insertBatch(insertList);
        if (dao::ERRORCODE_SUCCESS != ec) {
            std::ostringstream oss;
            std::string sep("");
            for (const auto& item : insertList) {
                oss << sep << item.uid();
                sep = ",";
            }
            LOGE << "failed to insert into group user, gid: " << req->gid 
                 << ", uids: " << oss.str() << ", error: " << ec;
            response.result(http::status::internal_server_error);
            return;
        }
        for (const auto& it : invitees) {
            std::vector<SimpleGroupMemberInfo> members;
            for (const auto& uid : it.second) {
                publishGroupUserEvent(req->gid, uid, INTERNAL_USER_ENTER_GROUP);
                members.emplace_back(uid, uid, GroupUser::ROLE_MEMBER);
            }
            GroupSysMsgBody msg{ENTER_GROUP, members};
            //get the inviter to pub the message
            pubGroupSystemMessage(it.first, req->gid, GROUP_MEMBER_UPDATE, nlohmann::json(msg).dump());
        }
        sendGroupKeysUpdateRequestWhenMemberChanges(account->uid(), req->gid, counter.memberCnt + insertList.size());
    }

    // delete those users from the pending list because the requests are complete
    if (!delUsers.empty()) {
        ec = m_pendingGroupUsers->del(req->gid, delUsers);
        if (dao::ERRORCODE_SUCCESS != ec) {
            std::string sep("");
            std::ostringstream oss;
            for (const auto& u : delUsers) {
                oss << sep << u;
                sep = ",";
            }
            LOGE << "failed to delete pending group user list, gid: " << req->gid << ", uids: " << oss.str();
            // response.result(http::status::internal_server_error);
            // return;
        }
    }
    
    response.result(http::status::ok);
    marker.setReturnCode(response.result_int());
}

void GroupManagerController::onUpdateGroupInfoV3(HttpContext& context)
{
    LOGT << "onUpdateGroupInfoV3 start...";
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName, "onUpdateGroupInfoV3");
    auto& res = context.response;
    res.result(http::status::ok);

    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& statics = context.statics;
    statics.setUid(account->uid());

    auto* updateGroupBody = boost::any_cast<UpdateGroupInfoBodyV3>(&context.requestEntity);
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

    LOGD << "get member role: " << account->uid() << ": " << gid << ": " << roleRet << ": " << role;
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
    LOGD << "update group data: " << account->uid() << ": " << gid << ", data: " << upData.dump();

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
        // do not return error in this case because we have already updated the group successfully
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

bool GroupManagerController::sendSwitchGroupKeysWithRetry(const std::string& uid, uint64_t gid, int64_t version)
{

#ifdef GROUP_EXCEPTION_INJECT_TEST
    if (m_groupConfig.groupConfigExceptionInject.disableGroupSwitchKeys) {
        LOGI << "USED GROUP_EXCEPTION_INJECT_TEST, disableGroupSwitchKeys, gid: " << gid << ", source: " << uid << ", version: " << version;
        return true;
    } else if (m_groupConfig.groupConfigExceptionInject.delayGroupSwitchKeysInMills > 0) {
        LOGI << "USED GROUP_EXCEPTION_INJECT_TEST, delayGroupSwitchKeysInMills: "
                << m_groupConfig.groupConfigExceptionInject.delayGroupSwitchKeysInMills
                << ", gid: " << gid << ", source: " << uid << ", version: " << version;
        boost::this_fiber::sleep_for(std::chrono::milliseconds(m_groupConfig.groupConfigExceptionInject.delayGroupSwitchKeysInMills));

    } else if (m_groupConfig.groupConfigExceptionInject.randomGroupSwitchKeysException) {
        int32_t percent = nowInMilli() % 100;
        do {
            if (percent <= m_groupConfig.groupConfigExceptionInject.randomGroupSwitchKeysSuccessPercent) {
                // NO-OP
                LOGI << "randomGroupSwitchKeysException, success! gid: " << gid << ", source: " << uid << ", version: " << version;
                break;
            }
            percent -= m_groupConfig.groupConfigExceptionInject.randomGroupSwitchKeysSuccessPercent;
            if (percent <= m_groupConfig.groupConfigExceptionInject.randomGroupSwitchKeysDelayPercent) {

                int32_t delayTimeInMills = random(m_groupConfig.groupConfigExceptionInject.randomGroupSwitchKeysDelayMinInMills,
                                                  m_groupConfig.groupConfigExceptionInject.randomGroupSwitchKeysDelayMaxInMills);
                LOGI << "randomGroupSwitchKeysException, delay! random time in mills: " << delayTimeInMills
                     << ", gid: " << gid << ", source: " << uid << ", version: " << version;
                boost::this_fiber::sleep_for(std::chrono::milliseconds(delayTimeInMills));
                break;
            }
            percent -= m_groupConfig.groupConfigExceptionInject.randomGroupSwitchKeysDelayPercent;
            if (percent <= m_groupConfig.groupConfigExceptionInject.randomGroupSwitchKeysDisablePercent) {
                LOGI << "randomGroupSwitchKeysException, disable! gid: " << gid << ", source: " << uid << ", version: " << version;
                return false;
            }
        } while (0);
    }
#endif

    int32_t retryTimes = 3;
    for (int i=1; i <= retryTimes; ++i) {
        GroupSwitchGroupKeysBody groupSwitchGroupKeysBody;
        groupSwitchGroupKeysBody.version = version;
        if (pubGroupSystemMessage(uid, gid, GroupMsg::TYPE_SWITCH_GROUP_KEYS, nlohmann::json(groupSwitchGroupKeysBody).dump())) {
            LOGI << "send switch group keys success, gid: " << gid << ", source: " << uid << ", version: " << version;
            MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "sendSwitchGroupKeys", 0, "success");
            return true;
        } else {
            LOGW << "send switch group keys fail, gid: " << gid << ", source: " << uid << ", version: " << version << ", retry times " << i;
            MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "sendSwitchGroupKeys", 0, "fail");
            boost::this_fiber::sleep_for(std::chrono::milliseconds(200 * i));
        }
    }
    LOGW << "send switch group keys fail, gid: " << gid << ", source: " << uid << ", version: " << version << ", used all retry times";
    return false;
}

bool GroupManagerController::sendGroupKeysUpdateRequestWithRetry(const std::string& uid, uint64_t gid, int32_t groupKeysMode)
{

#ifdef GROUP_EXCEPTION_INJECT_TEST
    if (m_groupConfig.groupConfigExceptionInject.disableGroupKeysUpdateRequest) {
        LOGI << "USED GROUP_EXCEPTION_INJECT_TEST, disableGroupKeysUpdateRequest, gid: " << gid << ", source: " << uid << ", groupKeysMode: " << groupKeysMode;
        return false;

    } else if (m_groupConfig.groupConfigExceptionInject.delayGroupKeysUpdateRequestInMills > 0) {
        LOGI << "USED GROUP_EXCEPTION_INJECT_TEST, delayGroupKeysUpdateRequestInMills: "
                << m_groupConfig.groupConfigExceptionInject.delayGroupKeysUpdateRequestInMills
                << ", gid: " << gid << ", source: " << uid << ", groupKeysMode: " << groupKeysMode;
        boost::this_fiber::sleep_for(std::chrono::milliseconds(m_groupConfig.groupConfigExceptionInject.delayGroupKeysUpdateRequestInMills));

    } else if (m_groupConfig.groupConfigExceptionInject.randomGroupKeysUpdateRequestException) {
        int32_t percent = nowInMilli() % 100;
        do {
            if (percent <= m_groupConfig.groupConfigExceptionInject.randomGroupKeysUpdateRequestSuccessPercent) {
                // NO-OP
                LOGI << "randomGroupKeysUpdateRequestException, success! gid: " << gid << ", source: " << uid << ", groupKeysMode: " << groupKeysMode;
                break;
            }
            percent -= m_groupConfig.groupConfigExceptionInject.randomGroupKeysUpdateRequestSuccessPercent;
            if (percent <= m_groupConfig.groupConfigExceptionInject.randomGroupKeysUpdateRequestDelayPercent) {

                int32_t delayTimeInMills = random(m_groupConfig.groupConfigExceptionInject.randomGroupKeysUpdateRequestDelayMinInMills,
                                                  m_groupConfig.groupConfigExceptionInject.randomGroupKeysUpdateRequestDelayMaxInMills);
                LOGI << "randomGroupKeysUpdateRequestException, delay! random time in mills: " << delayTimeInMills
                     << ", gid: " << gid << ", source: " << uid << ", groupKeysMode: " << groupKeysMode;
                boost::this_fiber::sleep_for(std::chrono::milliseconds(delayTimeInMills));
                break;
            }
            percent -= m_groupConfig.groupConfigExceptionInject.randomGroupKeysUpdateRequestDelayPercent;
            if (percent <= m_groupConfig.groupConfigExceptionInject.randomGroupKeysUpdateRequestDisablePercent) {
                LOGI << "randomUploadGroupKeysException, disable! gid: " << gid << ", source: " << uid << ", groupKeysMode: " << groupKeysMode;
                return false;
            }
        } while (0);
    }
#endif

    int32_t retryTimes = 3;
    for (int i=1; i <= retryTimes; ++i) {
        GroupUpdateGroupKeysRequestBody groupUpdateGroupKeysRequestBody;
        groupUpdateGroupKeysRequestBody.groupKeysMode = groupKeysMode;
        uint64_t mid;
        if (pubGroupSystemMessage(uid, gid, GroupMsg::TYPE_UPDATE_GROUP_KEYS_REQUEST, nlohmann::json(groupUpdateGroupKeysRequestBody).dump(), mid)) {
            LOGI << "send group keys update request success, gid: " << gid << ", source: " << uid << ", groupKeysMode: " << groupKeysMode;
            MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "sendGroupKeysUpdateRequest", 0, "success");
            if (groupKeysMode == static_cast<int32_t>(bcm::GroupKeys::ONE_FOR_EACH)) {
                // add keys cache
                std::vector<bcm::Keys> keys;
                if (m_accountsManager->getKeysByGid(gid, keys) == dao::ErrorCode::ERRORCODE_SUCCESS) {
                    m_keysCache.set(gid, mid, keys);
                }
            }
            return true;
        } else {
            LOGW << "send group keys update request fail, gid: " << gid << ", source: " << uid << ", groupKeysMode: " << groupKeysMode << ", retry times " << i;
            MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsGmanagerServiceName, "sendGroupKeysUpdateRequest", 0, "fail");
            boost::this_fiber::sleep_for(std::chrono::milliseconds(200 * i));
        }
    }
    LOGW << "send group keys update request fail, gid: " << gid << ", source: " << uid << ", groupKeysMode: " << groupKeysMode << ", used all retry times";
    return false;
}

bool GroupManagerController::sendGroupKeysUpdateRequestWhenMemberChanges(const std::string& uid,
                                                                         uint64_t gid,
                                                                         uint32_t groupMembersAfterChanges)
{

    auto sendPowerGroupKeysChanges = [&]() -> bool {
        LOGI << "send power group keys update request, gid: " << gid << ", source: " << uid << ", groupMembersAfterChanges: " << groupMembersAfterChanges;
        return sendGroupKeysUpdateRequestWithRetry(uid, gid, GroupKeys::ONE_FOR_EACH);
    };

    auto sendNormalGroupKeysChanges = [&]() -> bool {
        LOGI << "send normal group keys update request, gid: " << gid << ", source: " << uid << ", groupMembersAfterChanges: " << groupMembersAfterChanges;
        return sendGroupKeysUpdateRequestWithRetry(uid, gid, GroupKeys::ALL_THE_SAME);
    };

    // -----|--------------|----------------|---------------
    //     min            max    normalGroupRefreshKeysMax
    //
    // 1. (-,min], power group, change keys every time
    // 2. (min, max], if current is power group, change power group keys, else change normal group keys.
    // 3. (max, normalGroupRefreshKeysMax], normal group, change normal group keys every time
    // 4. (normalGroupRefreshKeysMax, --), normal group, if current is power group, change group keys, else not

    // 1.(-,min], power group
    if (groupMembersAfterChanges <= m_groupConfig.powerGroupMin) {
        return sendPowerGroupKeysChanges();

    // 2. (min, max]
    } else if (groupMembersAfterChanges > m_groupConfig.powerGroupMin && groupMembersAfterChanges <= m_groupConfig.powerGroupMax){
        // get current mode
        GroupKeys::GroupKeysMode mode;
        auto ret = m_groupKeys->getLatestMode(gid, mode);
        if (ret != dao::ERRORCODE_SUCCESS) {
            LOGE << "failed to query the latest group keys mode, gid: " << gid << " ret:" << ret;
            mode = GroupKeys::UNKNOWN;
        }
        if (mode == GroupKeys::ONE_FOR_EACH) {
            return sendPowerGroupKeysChanges();
        } else if (mode == GroupKeys::ALL_THE_SAME) {
            return sendNormalGroupKeysChanges();
        } else if (mode == GroupKeys::UNKNOWN) {
            // we can change the group keys when fail to fetch latest mode
            // it will lead to change to power group
            // but, it will avoid the condition that :
            // one join the group the group keys not changes, and, that one will never get the group keys
            return sendPowerGroupKeysChanges();
        }

    // 3. (max, normalGroupRefreshKeysMax]
    } else if (groupMembersAfterChanges > m_groupConfig.powerGroupMax && groupMembersAfterChanges <= m_groupConfig.normalGroupRefreshKeysMax) {
        return sendNormalGroupKeysChanges();

    // 4. (normalGroupRefreshKeysMax, --)
    } else {
        // get current mode
        GroupKeys::GroupKeysMode mode;
        auto ret = m_groupKeys->getLatestMode(gid, mode);
        if (ret != dao::ERRORCODE_SUCCESS) {
            LOGE << "failed to query the latest group keys mode, gid: " << gid << " ret:" << ret;
            mode = GroupKeys::UNKNOWN;
        }
        if (mode == GroupKeys::ONE_FOR_EACH) {
            return sendNormalGroupKeysChanges();
        } else if (mode == GroupKeys::ALL_THE_SAME) {
            // no op
            LOGI << "(normalGroupRefreshKeysMax, --), last group keys mode is normal group, will not change keys, gid: "
                    << gid << ", source: " << uid << ", groupMembersAfterChanges: " << groupMembersAfterChanges;
            return true;
        } else if (mode == GroupKeys::UNKNOWN) {
            // we can change the group keys when fail to fetch latest mode
            // fire normal group keys update again.
            return sendNormalGroupKeysChanges();
        }
    }
    LOGE << "should not reach here(sendGroupKeysUpdateRequestWhenMemberChanges)";
    return false;
}

std::string GroupManagerController::getMemberJoinLimiterId(uint64_t gid, const std::string& uid)
{
    std::ostringstream oss;
    oss << gid << "_" << uid;
    return oss.str();
}

bool GroupManagerController::pubGroupSystemMessage(const std::string& uid, 
                                                   const uint64_t groupid, 
                                                   const int type, 
                                                   const std::string& strText,
                                                   uint64_t& mid)
{
    LOGI << "request to send group system message.(uid:" << uid << " gid:" << groupid << " type:" << type << " text:" << strText << ")";

    long nowTime = nowInMilli();

    //format event message.
    GroupMsg groupMessage;
    groupMessage.set_gid(groupid);
    groupMessage.set_fromuid(uid);
    groupMessage.set_text(strText);
    groupMessage.set_updatetime(nowTime);
    groupMessage.set_createtime(nowTime);
    groupMessage.set_type(static_cast<GroupMsg::Type>(type));
    groupMessage.set_status(1);
    groupMessage.set_atlist("[]");

    //write group event message to db.
    uint64_t newMid = 0;
    if (m_groupMessage->insert(groupMessage, newMid) != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to save group message to db.(gid:" << groupid << " uid:" << uid << " type:" << type
             << " newmid: " << newMid << " )";
        return false;
    }
    mid = newMid;

    LOGD << "insert message success: " << newMid << ": " << groupMessage.Utf8DebugString();

    nlohmann::json jsMessage = nlohmann::json{
                                    {"gid", groupid},
                                    {"mid", newMid},
                                    {"type", static_cast<int>(type)},
                                    {"from_uid", uid},
                                    {"create_time", nowTime},
                                    {"text", strText}
                                    };
    //std::string strChannel = "group_3_" + std::to_string(groupid);
    std::string strChannel = "group_event_msg";
    OnlineRedisManager::Instance()->publish(strChannel, jsMessage.dump(),[strChannel](int status, const redis::Reply& reply) {
        if (REDIS_OK != status || !reply.isInteger()) {
            LOGE << "failed to publish group system message to redis, channel: " << strChannel << ", status: " << status;
            return;
        }
        
        if (reply.getInteger() > 0) {
            LOGI << "success to publish group system message to redis, channel: "
                    << strChannel << ", subscribe: " << reply.getInteger();
        } else {
            LOGE << "failed to publish group system message to redis, channel: "
                    << strChannel << ", subscribe: " << reply.getInteger();
        }
    });

    return true;
}

int32_t GroupManagerController::random(int32_t min, int32_t max) {
    static bool first = true;
    if (first)
    {
        srand( time(NULL) ); //seeding for the first time only!
        first = false;
    }
    return min + rand() % (( max + 1 ) - min);
}

bool GroupManagerController::KeysCache::get(uint64_t gid, int64_t version, std::vector<bcm::Keys>& keys)
{
    std::string key = cacheKey(gid, version);
    std::string value;
    if (!m_cache->get(key, value) || value.empty()) {
        return false;
    }
    LOGT << "get keys cache, key: " << key;
    return deserialize(value, keys);
}

bool GroupManagerController::KeysCache::set(uint64_t gid, int64_t version, const std::vector<bcm::Keys>& keys)
{
    if (keys.empty()) {
        return false;
    }
    std::string key = cacheKey(gid, version);
    std::string value;
    if (!serialize(keys, value)) {
        return false;
    }
    LOGT << "set keys cache, key: " << key;
    return m_cache->set(key, value);
}

std::string GroupManagerController::KeysCache::cacheKey(uint64_t gid, int64_t version)
{
    std::ostringstream oss;
    oss << gid << "_" << version;
    return oss.str();
}

bool GroupManagerController::KeysCache::serialize(const std::vector<bcm::Keys>& keys, std::string& value)
{
    if (keys.empty()) {
        return false;
    }
    size_t length = 0;
    for (const auto& key : keys) {
        length += sizeof(uint32_t);
        length += key.ByteSize();
    }
    value.resize(length);

    google::protobuf::io::ArrayOutputStream aos(&value[0], value.size());
    google::protobuf::io::CodedOutputStream cos(&aos);
    for (const auto& key : keys) {
        cos.WriteLittleEndian32(key.ByteSize());
        if (!key.SerializeToCodedStream(&cos)) {
            return false;
        }
    }
    return true;
}

bool GroupManagerController::KeysCache::deserialize(const std::string& value, std::vector<bcm::Keys>& keys)
{
    size_t pos = 0;
    while (pos < value.size()) {
        uint32_t len;
        google::protobuf::io::CodedInputStream::ReadLittleEndian32FromArray((const uint8_t*)&value[pos], &len);
        pos += sizeof(len);
        google::protobuf::io::ArrayInputStream ais(&value[pos], len);
        google::protobuf::io::CodedInputStream cis(&ais);
        
        bcm::Keys key;
        if (!key.ParseFromCodedStream(&cis)) {
            return false;
        }
        keys.emplace_back(std::move(key));
        pos += len;
    }
    return true;
}

void GroupManagerController::onQueryMembersV3(HttpContext& context)
{
    ExecTimeAndReturnCodeMarker marker(kMetricsGmanagerServiceName,
        "onQueryMembersV3");

    QueryMembersRequestV3* request = boost::any_cast<QueryMembersRequestV3>(&context.requestEntity);
    auto& response = context.response;
    Account* account = boost::any_cast<Account>(&context.authResult);

    std::string sep("");
    std::stringstream ss;
    ss << "[";
    for (const auto& u : request->uids) {
        ss << sep << u;
        sep = ",";
    }
    ss << "]";

    LOGI << "receive to query group member list.(uid: " << account->uid()
         << " gid: " << request->gid << " uids: " << ss.str() << ")";
    
    if (!request->check()) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, error);
#endif
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        return;
    }

    GroupUser user;
    dao::ErrorCode ec = m_groupUsers->getMember(request->gid, account->uid(), user);
    if (dao::ERRORCODE_NO_SUCH_DATA == ec) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, "group not existed or you are not in this group");
#endif
        response.result(http::status::bad_request);
        marker.setReturnCode(response.result_int());
        LOGE << "there is no user " << account->uid() << " in group " << request->gid; 
        return;
    }
    if (dao::ERRORCODE_SUCCESS != ec) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, "database error when getting member info for uid " + account->uid());
#endif
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        LOGE << "failed to query my group user info.(myuid: " << account->uid()
             << " gid: "<< request->gid << "): " << ec;
        return;
    }

    std::vector<GroupUser> members;
    ec = m_groupUsers->getMemberBatch(request->gid, 
                                      request->uids,
                                      members);

    if (dao::ERRORCODE_NO_SUCH_DATA != ec && dao::ERRORCODE_SUCCESS != ec) {
#ifndef NDEBUG
        //context.responseEntity = GroupResponse(group::ERRORCODE_SUCCESS, "database error when getting members for this group");
#endif
        response.result(http::status::internal_server_error);
        marker.setReturnCode(response.result_int());
        LOGE << "failed to read group member list.(myuid: " << account->uid()
             << " gid: "<< request->gid << "): " << ec;
        return;
    }

    QueryMembersResponseV3 responseEntity;
    for (auto it = members.begin(); it != members.end(); ++it) {
        GroupMember m;
        m.uid = it->uid();
        m.nick = it->nick();
        m.nickname = it->nickname();
        m.groupNickname = it->groupnickname();
        m.profileKeys = it->profilekeys();
        m.role = static_cast<int32_t>(it->role());
        m.createTime = it->createtime();
        m.proof = it->proof();
        responseEntity.members.push_back(m);
    }

    context.responseEntity = responseEntity;
    response.result(http::status::ok);
    marker.setReturnCode(response.result_int());
}


}
