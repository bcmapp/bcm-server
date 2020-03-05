#pragma once

#include <brpc/channel.h>
#include "dao/group_msgs.h"
#include "proto/brpc/rpc_group_msg.pb.h"
#include <memory>
#include <atomic>

namespace bcm {
namespace dao {


class GroupMsgsRpcImp : public GroupMsgs {
public:
    GroupMsgsRpcImp(brpc::Channel* ch);

    virtual ErrorCode insert(const bcm::GroupMsg& msg, uint64_t& mid);

    /*
     * return ERRORCODE_SUCCESS if get msg success
     * corresponds to getGroupMsg
     */
    virtual ErrorCode get(uint64_t groupId, uint64_t mid, GroupMsg& msg);

    /*
     * return ERRORCODE_SUCCESS if get msgs success
     * corresponds to getGroupMsgRange
     */
    virtual ErrorCode batchGet(uint64_t groupId, uint64_t from, uint64_t to,
                               uint64_t limit, bcm::GroupUser::Role role,
                               bool supportRrecall, std::vector<bcm::GroupMsg>& msgs);

    /*
     * return ERRORCODE_SUCCESS if recall success
     * corresponds to recallGroupMsg
     */
    virtual ErrorCode recall(const std::string& sourceExtra,
                             const std::string& uid,
                             uint64_t gid,
                             uint64_t mid,
                             uint64_t& newMid);

private:
    static std::atomic<uint64_t> logId;
    bcm::dao::rpc::GroupMsgsService_Stub stub;
};

}  // namespace dao
}  // namespace bcm

