#include "utils/log.h"
#include "group_user_rpc_impl.h"

namespace bcm {
namespace dao {

GroupUsersRpcImpl::GroupUsersRpcImpl(brpc::Channel* ch) : stub(ch)
{

}

ErrorCode GroupUsersRpcImpl::insert(const bcm::GroupUser& user)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::InsertGroupUserReq request;
    bcm::dao::rpc::InsertGroupUserResp response;
    brpc::Controller cntl;

    ::bcm::GroupUser* p = request.mutable_groupuser();
    if (!p) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
    *p = user;
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.insert(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::insertBatch(const std::vector<bcm::GroupUser>& users)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::BatchInsertGroupUserReq request;
    bcm::dao::rpc::BatchInsertGroupUserResp response;
    brpc::Controller cntl;

    for (const auto& u : users) {
        ::bcm::GroupUser* p = request.add_groupuser();
        if (!p) {
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        *p = u;
    }
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.insertBatch(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::getMemberRole(uint64_t gid, const std::string& uid, bcm::GroupUser::Role& role)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetGroupMemberRoleReq request;
    bcm::dao::rpc::GetGroupMemberRoleResp response;
    brpc::Controller cntl;
    request.set_groupid(gid);
    std::string* p = request.add_uid();
    if (!p) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
    *p = uid;
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getMemberRoles(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            for (const auto& ur : response.userroles()) {
                if (ur.uid() == uid) {
                    role = ur.role();
                }
            }
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::getMemberRoles(uint64_t gid, std::map <std::string, bcm::GroupUser::Role>& userRoles)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetGroupMemberRoleReq request;
    bcm::dao::rpc::GetGroupMemberRoleResp response;
    brpc::Controller cntl;
    request.set_groupid(gid);
    for (const auto& ur : userRoles) {
        std::string* p = request.add_uid();
        if (!p) {
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        *p = ur.first;
    }
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getMemberRoles(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            for (const auto& ur : response.userroles()) {
                userRoles[ur.uid()] = ur.role();
            }
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::delMember(uint64_t gid, const std::string& uid)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::DeleteGroupMembersReq request;
    bcm::dao::rpc::DeleteGroupMembersResp response;
    brpc::Controller cntl;
    request.set_groupid(gid);
    std::string* p = request.add_uids();
    if (!p) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
    *p = uid;
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.delMemberBatch(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::delMemberBatch(uint64_t gid, const std::vector <std::string>& uids)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::DeleteGroupMembersReq request;
    bcm::dao::rpc::DeleteGroupMembersResp response;
    brpc::Controller cntl;
    request.set_groupid(gid);
    for (const auto u : uids) {
        std::string* p = request.add_uids();
        if (!p) {
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        *p = u;
    }
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.delMemberBatch(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::getMemberBatch(uint64_t gid,
                                            const std::vector <std::string>& uids,
                                            std::vector <bcm::GroupUser>& users)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetGroupMemberBatchReq request;
    bcm::dao::rpc::GetGroupMemberBatchResp response;
    brpc::Controller cntl;
    request.set_groupid(gid);
    for (const auto u : uids) {
        std::string* p = request.add_uids();
        if (!p) {
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        *p = u;
    }
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getMemberInfoBatch(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            for (const auto& gu : response.users()) {
                users.emplace_back(gu);
            }
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::getMemberRangeByRolesBatch(uint64_t gid,
                                                        const std::vector <bcm::GroupUser::Role>& roles,
                                                        std::vector <bcm::GroupUser>& users)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::BatchGetGroupMemberByRoleReq request;
    bcm::dao::rpc::BatchGetGroupMemberByRoleResp response;
    brpc::Controller cntl;
    request.set_groupid(gid);
    for (const auto& r :roles) {
        request.add_roles(r);
    }
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getMemberRangeByRolesBatch(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            for (const auto& gu : response.users()) {
                users.emplace_back(gu);
            }
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::getJoinedGroupsList(const std::string& uid, std::vector <UserGroupDetail>& groups)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetJoinedGroupsListReq request;
    bcm::dao::rpc::GetJoinedGroupsListResp response;
    brpc::Controller cntl;
    request.set_uid(uid);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getJoinedGroupsList(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            for (const auto& ugd : response.groups()) {
                UserGroupDetail detail;
                detail.group = ugd.group();
                detail.user = ugd.user();
                detail.counter.owner = ugd.counter().owner();
                detail.counter.memberCnt = ugd.counter().membercnt();
                detail.counter.subscriberCnt = ugd.counter().subscribercnt();
                groups.emplace_back(detail);
            }
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::getJoinedGroups(const std::string& uid, std::vector <uint64_t>& gids)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetJoinedGroupsReq request;
    bcm::dao::rpc::GetJoinedGroupsResp response;
    brpc::Controller cntl;
    request.set_uid(uid);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getJoinedGroups(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            for (const auto& gid : response.gids()) {
                gids.emplace_back(gid);
            }
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::getGroupDetailByGid(uint64_t gid, const std::string& uid, UserGroupDetail& detail)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetGroupDetailByGidReq request;
    bcm::dao::rpc::GetGroupDetailByGidResp response;
    brpc::Controller cntl;
    request.set_groupid(gid);
    request.set_uid(uid);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getGroupDetailByGid(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            detail.group = response.detail().group();
            detail.user = response.detail().user();
            detail.counter.owner = response.detail().counter().owner();
            detail.counter.memberCnt = response.detail().counter().membercnt();
            detail.counter.subscriberCnt = response.detail().counter().subscribercnt();
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::getGroupOwner(uint64_t gid, std::string& owner)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetGroupOwnerReq request;
    bcm::dao::rpc::GetGroupOwnerResp response;
    brpc::Controller cntl;
    request.set_groupid(gid);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getGroupOwner(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            owner = response.owner();
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::getMember(uint64_t gid, const std::string& uid, bcm::GroupUser& user)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetMemberReq request;
    bcm::dao::rpc::GetMemberResp response;
    brpc::Controller cntl;
    request.set_groupid(gid);
    request.set_uid(uid);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getMember(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            user = response.user();
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::queryGroupMemberInfoByGid(uint64_t gid, GroupCounter& counter)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::QueryGroupCountInfoByGidReq request;
    bcm::dao::rpc::QueryGroupCountInfoByGidResp response;
    brpc::Controller cntl;
    request.set_groupid(gid);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.queryGroupCountInfoByGid(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            counter.owner = response.counter().owner();
            counter.memberCnt = response.counter().membercnt();
            counter.subscriberCnt = response.counter().subscribercnt();
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::queryGroupMemberInfoByGid(uint64_t gid,
                                                       GroupCounter& counter,
                                                       const std::string& querier,
                                                       bcm::GroupUser::Role& querierRole,
                                                       const std::string& nextOwner,
                                                       bcm::GroupUser::Role& nextOwnerRole)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::QueryGroupMemberInfoByGidReq request;
    bcm::dao::rpc::QueryGroupMemberInfoByGidResp response;
    brpc::Controller cntl;
    request.set_groupid(gid);
    request.set_querier(querier);
    request.set_nextowner(nextOwner);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.queryGroupMemberInfoByGid(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            counter.owner = response.counter().owner();
            counter.memberCnt = response.counter().membercnt();
            counter.subscriberCnt = response.counter().subscribercnt();
            querierRole = response.querierrole();
            nextOwnerRole = response.nextownerrole();
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::update(uint64_t gid, const std::string& uid, const nlohmann::json& upData)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::UpdateGroupUserReq request;
    bcm::dao::rpc::UpdateGroupUserResp response;
    brpc::Controller cntl;
    request.set_groupid(gid);
    request.set_uid(uid);
    request.set_jsonfield(upData.dump());
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.update(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::getMemberRangeByRolesBatchWithOffset(uint64_t gid,
                                                                  const std::vector<bcm::GroupUser::Role>& roles,
                                                                  const std::string& startUid,
                                                                  int count,
                                                                  std::vector<bcm::GroupUser>& users)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::PageGetGroupMemberByRoleReq request;
    bcm::dao::rpc::PageGetGroupMemberByRoleResp response;
    brpc::Controller cntl;
    request.set_groupid(gid);
    for (const auto& r :roles) {
        request.add_roles(r);
    }
    request.set_startuid(startUid);
    request.set_count(count);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getMemberRangeByRolesPage(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            for (const auto& gu : response.users()) {
                users.emplace_back(gu);
            }
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::getGroupDetailByGidBatch(const std::vector<uint64_t>& gids, const std::string& uid,
                                                      std::vector<UserGroupEntry>& entries)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::BatchGetGroupDetailByGidReq request;
    bcm::dao::rpc::BatchGetGroupDetailByGidResp response;
    brpc::Controller cntl;
    for (const auto& g : gids) {
        request.add_groupids(g);
    }
    request.set_uid(uid);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getGroupDetailByGidBatch(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            for (const auto& it : response.details()) {
                UserGroupEntry entry;
                entry.group = it.group();
                entry.user = it.user();
                entry.owner = it.counter().owner();
                entries.emplace_back(entry);
            }
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::updateIfEmpty(uint64_t gid, const std::string& uid, const nlohmann::json& upData)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::UpdateGroupUserReq request;
    bcm::dao::rpc::UpdateGroupUserResp response;
    brpc::Controller cntl;
    request.set_groupid(gid);
    request.set_uid(uid);
    request.set_jsonfield(upData.dump());
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.updateIfEmpty(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupUsersRpcImpl::getMembersOrderByCreateTime(uint64_t gid,
                                                         const std::vector<bcm::GroupUser::Role>& roles,
                                                         const std::string& startUid,
                                                         int64_t createTime,
                                                         int count,
                                                         std::vector<bcm::GroupUser>& users)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetMembersOrderByCreateTimeReq request;
    bcm::dao::rpc::GetMembersOrderByCreateTimeResp response;
    brpc::Controller cntl;
    for (const auto& role : roles) {
        request.add_roles(role);
    }
    request.set_groupid(gid);
    request.set_startuid(startUid);
    request.set_createtime(createTime);
    request.set_count(count);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getMembersOrderByCreateTime(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            users.assign(response.users().begin(), response.users().end());
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

} 

std::atomic <uint64_t> GroupUsersRpcImpl::logId(0);

}
}
