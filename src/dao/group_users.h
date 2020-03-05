#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include "proto/dao/group_user.pb.h"
#include "proto/dao/group.pb.h"
#include "proto/dao/error_code.pb.h"
#include "proto/dao/account.pb.h"

namespace bcm {
namespace dao {

struct GroupCounter {
    std::string owner;
    uint32_t memberCnt;
    uint32_t subscriberCnt;
};

struct UserGroupDetail {
    bcm::Group group;
    bcm::GroupUser user;
    GroupCounter counter;
};

struct UserGroupEntry {
    bcm::Group group;
    bcm::GroupUser user;
    std::string owner;
};

/*
 * Accounts dao virtual base class
 */
class GroupUsers {
public:
    /*
     * return ERRORCODE_SUCCESS if insert group user success
     * corresponds to insertGroupUsers
     */
    virtual ErrorCode insert(const bcm::GroupUser& user) = 0;

    /*
     * return ERRORCODE_SUCCESS if insert group user success
     * corresponds to createNewGroupMembers in group_handler
     */

    virtual ErrorCode insertBatch(const std::vector<bcm::GroupUser>& users) = 0;

    /*
     * return ERRORCODE_SUCCESS if get role success
     * corresponds to getGroupMemberRole
     */
    virtual ErrorCode getMemberRole(uint64_t gid, const std::string& uid, bcm::GroupUser::Role& role) = 0;

    /*
     * return ERRORCODE_SUCCESS if get all member roles success
     * corresponds to getGroupMemberRoleBatch
     */
    virtual ErrorCode getMemberRoles(uint64_t gid, std::map <std::string, bcm::GroupUser::Role>& userRoles) = 0;

    /*
     * return ERRORCODE_SUCCESS if del member success
     * corresponds to delGroupMember
     */
    virtual ErrorCode delMember(uint64_t gid, const std::string& uid) = 0;

    /*
     * return ERRORCODE_SUCCESS if del members success
     * corresponds to delGroupMemberBatch
     */
    virtual ErrorCode delMemberBatch(uint64_t gid, const std::vector <std::string>& uids) = 0;

    /*
     * return ERRORCODE_SUCCESS if get success
     * corresponds to queryGroupMemberBatch
     */
    virtual ErrorCode getMemberBatch(uint64_t gid, const std::vector <std::string>& uids,
                                     std::vector <bcm::GroupUser>& users) = 0;

    /*
     * return ERRORCODE_SUCCESS if get success
     * corresponds to queryUsersByRolesBatch
     */
    virtual ErrorCode getMemberRangeByRolesBatch(uint64_t gid,
                                                 const std::vector <bcm::GroupUser::Role>& roles,
                                                 std::vector <bcm::GroupUser>& users) = 0;

    /**
     * return ERRORCODE_SUCCESS if get success
     * corresponds to queryUsersByRolesBatch
     */
    virtual ErrorCode getMemberRangeByRolesBatchWithOffset(uint64_t gid,
                                                           const std::vector <bcm::GroupUser::Role>& roles,
                                                           const std::string& startUid, int count,
                                                           std::vector <bcm::GroupUser>& users) = 0;

    /*
     * return ERRORCODE_SUCCESS if get groups success
     * corresponds to queryGroupListByUid
     */
    virtual ErrorCode getJoinedGroupsList(const std::string& uid, std::vector <UserGroupDetail>& groups) = 0;

    /*
     * return ERRORCODE_SUCCESS if get groups success
     * corresponds to queryGroupGid
     */
    virtual ErrorCode getJoinedGroups(const std::string& uid, std::vector <uint64_t>& gids) = 0;

    /*
     * return ERRORCODE_SUCCESS if get success
     * corresponds to queryGroupInfoByGid
     */
    virtual ErrorCode getGroupDetailByGid(uint64_t gid, const std::string& uid, UserGroupDetail& detail) = 0;

    /*
     * return ERRORCODE_SUCCESS if get success
     * corresponds to queryGroupInfoByGid
     */
    virtual ErrorCode getGroupDetailByGidBatch(const std::vector<uint64_t>& gids, 
                                               const std::string& uid, 
                                               std::vector<UserGroupEntry>& entries) = 0;

    /*
     * return ERRORCODE_SUCCESS if get group owner success
     * corresponds to getGroupOwner
     */
    virtual ErrorCode getGroupOwner(uint64_t gid, std::string& owner) = 0;

    /*
     * return ERRORCODE_SUCCESS if get success
     * corresponds to queryUsersinGroupInfo
     * corresponds to queryGroupUsersTable
     * corresponds to queryGroupUserInfo
     */
    virtual ErrorCode getMember(uint64_t gid, const std::string& uid, bcm::GroupUser& user) = 0;

    /*
     * return ERRORCODE_SUCCESS if query success
     * corresponds to queryGroupMemberInfoByGid
     */
    virtual ErrorCode queryGroupMemberInfoByGid(uint64_t gid, GroupCounter& counter) = 0;

    /*
     * return ERRORCODE_SUCCESS if query success
     * corresponds to queryGroupMemberInfoByGid
     */
    virtual ErrorCode queryGroupMemberInfoByGid(uint64_t gid,
                                                GroupCounter& counter,
                                                const std::string& querier,
                                                bcm::GroupUser::Role& querierRole,
                                                const std::string& nextOwner,
                                                bcm::GroupUser::Role& nextOwnerRole) = 0;

    /*
     * return ERRORCODE_SUCCESS if update success
     * corresponds to updateGroupUsersTable
     */
    virtual ErrorCode update(uint64_t gid, const std::string& uid, const nlohmann::json& upData) = 0;

    /**
     * @brief if the keys in upData are empty, then update them
     * @param gid
     * @param uid
     * @param upData - key-value pairs to be updated
     * @return - ERRORCODE_SUCCESS if sucess, otherwise fail
     */
    virtual ErrorCode updateIfEmpty(uint64_t gid, const std::string& uid, const nlohmann::json& upData) = 0;

    /**
     * @brief get members order by (create_time, uid)
     * @param gid 
     * @param roles 
     * @param startUid - uid of the start point (create_time, uid)
     * @param createTime - create_time of the start point (create_time, uid)
     * @param count - fetch count start from the (create_time, uid)
     * @param users 
     * @return 
     */
    virtual ErrorCode getMembersOrderByCreateTime(uint64_t gid,
                                                  const std::vector<bcm::GroupUser::Role>& roles,
                                                  const std::string& startUid,
                                                  int64_t createTime,
                                                  int count,
                                                  std::vector<bcm::GroupUser>& users) = 0; 

};

}  // namespace dao
}  // namespace bcm
