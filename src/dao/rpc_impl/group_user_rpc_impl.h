#pragma once

#include <brpc/channel.h>
#include "dao/group_users.h"
#include "proto/brpc/rpc_group_user.pb.h"
#include <memory>
#include <atomic>


namespace bcm {
namespace dao {

class GroupUsersRpcImpl : public GroupUsers {
public:
    GroupUsersRpcImpl(brpc::Channel* ch);

    virtual ErrorCode insert(const bcm::GroupUser& user) override;

    virtual ErrorCode insertBatch(const std::vector<bcm::GroupUser>& users);

    virtual ErrorCode getMemberRole(uint64_t gid, const std::string& uid, bcm::GroupUser::Role& role) override;

    virtual ErrorCode getMemberRoles(uint64_t gid, std::map <std::string, bcm::GroupUser::Role>& userRoles) override;

    virtual ErrorCode delMember(uint64_t gid, const std::string& uid) override;

    virtual ErrorCode delMemberBatch(uint64_t gid, const std::vector <std::string>& uids) override;

    virtual ErrorCode getMemberBatch(uint64_t gid,
                                     const std::vector <std::string>& uids,
                                     std::vector <bcm::GroupUser>& users) override;

    virtual ErrorCode getMemberRangeByRolesBatch(uint64_t gid, const std::vector <bcm::GroupUser::Role>& roles,
                                                 std::vector <bcm::GroupUser>& users) override;

    virtual ErrorCode getMemberRangeByRolesBatchWithOffset(uint64_t gid,
                                                           const std::vector <bcm::GroupUser::Role>& roles,
                                                           const std::string& startUid, int count,
                                                           std::vector <bcm::GroupUser>& users);

    virtual ErrorCode getJoinedGroupsList(const std::string& uid, std::vector <UserGroupDetail>& groups) override;

    virtual ErrorCode getJoinedGroups(const std::string& uid, std::vector <uint64_t>& gids) override;

    virtual ErrorCode getGroupDetailByGid(uint64_t gid, const std::string& uid, UserGroupDetail& detail) override;

    virtual ErrorCode getGroupDetailByGidBatch(const std::vector<uint64_t>& gids, const std::string& uid,
                                               std::vector<UserGroupEntry>& entries);

    virtual ErrorCode getGroupOwner(uint64_t gid, std::string& owner) override;

    virtual ErrorCode getMember(uint64_t gid, const std::string& uid, bcm::GroupUser& user) override;

    virtual ErrorCode queryGroupMemberInfoByGid(uint64_t gid, GroupCounter& counter) override;

    virtual ErrorCode queryGroupMemberInfoByGid(uint64_t gid, GroupCounter& counter, const std::string& querier,
                                                bcm::GroupUser::Role& querierRole, const std::string& nextOwner,
                                                bcm::GroupUser::Role& nextOwnerRole) override;

    virtual ErrorCode update(uint64_t gid, const std::string& uid, const nlohmann::json& upData) override;

    virtual ErrorCode updateIfEmpty(uint64_t gid, const std::string& uid, const nlohmann::json& upData) override;

    virtual ErrorCode getMembersOrderByCreateTime(uint64_t gid,
                                                  const std::vector<bcm::GroupUser::Role>& roles,
                                                  const std::string& startUid,
                                                  int64_t createTime,
                                                  int count,
                                                  std::vector<bcm::GroupUser>& users) override; 
                                                  
    
private:
    static std::atomic <uint64_t> logId;
    bcm::dao::rpc::GroupUserService_Stub stub;
};

}  // namespace dao
}  // namespace bcm
