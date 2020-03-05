#pragma once

#include <string>
#include <vector>
#include "proto/dao/group_msg.pb.h"
#include "proto/dao/group_user.pb.h"
#include "proto/dao/error_code.pb.h"

namespace bcm {
namespace dao {

/*
 * Accounts dao virtual base class
 */
class GroupMsgs {
public:
    /*
     * return ERRORCODE_SUCCESS if insert group msg success
     * corresponds to insertGroupMessage
     */
    virtual ErrorCode insert(const bcm::GroupMsg& msg, uint64_t& mid) = 0;

    /*
     * return ERRORCODE_SUCCESS if get msg success
     * corresponds to getGroupMsg
     */
    virtual ErrorCode get(uint64_t groupId, uint64_t mid, GroupMsg& msg) = 0;

    /*
     * return ERRORCODE_SUCCESS if get msgs success
     * corresponds to getGroupMsgRange
     */
    virtual ErrorCode batchGet(uint64_t groupId, uint64_t from, uint64_t to, uint64_t limit, bcm::GroupUser::Role role, bool supportRrecall, std::vector<bcm::GroupMsg>& msgs) = 0;

    /*
     * return ERRORCODE_SUCCESS if recall success
     * corresponds to recallGroupMsg
     */
    virtual ErrorCode recall(const std::string& sourceExtra, const std::string& uid, uint64_t gid, uint64_t mid, uint64_t& newMid) = 0;
};

}  // namespace dao
}  // namespace bcm
