//
// Created by shu wang on 2019/1/1.
//

#include <map>

#include "utils/log.h"

#include "group_msg_rpc_impl.h"
#include "proto/brpc/rpc_group_msg.pb.h"

namespace bcm {
namespace dao {

std::atomic<uint64_t> GroupMsgsRpcImp::logId(0);

GroupMsgsRpcImp::GroupMsgsRpcImp(brpc::Channel* ch) : stub(ch)
{

}

ErrorCode GroupMsgsRpcImp::insert(const bcm::GroupMsg& msg, uint64_t& mid)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::InsertGroupMessageReq request;
    bcm::dao::rpc::InsertGroupMessageResp response;
    brpc::Controller cntl;

    bcm::GroupMsg* newGroupMsg = request.mutable_msg();
    *newGroupMsg = msg;

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.insert(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << " , req: " << request.Utf8DebugString();
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            mid = response.newmid();
        }

        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what()
             << " , req: " << request.Utf8DebugString();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

/*
 * return ERRORCODE_SUCCESS if get msg success
 * corresponds to getGroupMsg
 */
ErrorCode GroupMsgsRpcImp::get(uint64_t groupId, uint64_t mid, GroupMsg& msg)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetGroupMsgReq request;
    bcm::dao::rpc::GetGroupMsgResp response;
    brpc::Controller cntl;

    request.set_groupid(groupId);
    request.set_mid(mid);

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.get(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << " , req: " << request.Utf8DebugString();
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        msg = response.msg();

        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << ex.what()
             << " , req: " << request.Utf8DebugString();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

/*
 * return ERRORCODE_SUCCESS if get msgs success
 * corresponds to getGroupMsgRange
 */
ErrorCode GroupMsgsRpcImp::batchGet(uint64_t groupId, uint64_t from, uint64_t to,
                           uint64_t limit, bcm::GroupUser::Role role,
                           bool supportRrecall, std::vector<bcm::GroupMsg>& msgs)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::BatchGetGroupMsgReq request;
    bcm::dao::rpc::BatchGetGroupMsgResp response;
    brpc::Controller cntl;

    request.set_groupid(groupId);
    request.set_from(from);
    request.set_to(to);
    request.set_limit(limit);
    request.set_role(role);
    request.set_supportrecall(supportRrecall);

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.batchGet(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << " , req: " << request.Utf8DebugString();
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            msgs.clear();
            msgs.assign(response.msgs().begin(), response.msgs().end());
        }

        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what()
             << " , req: " << request.Utf8DebugString();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

/*
 * return ERRORCODE_SUCCESS if recall success
 * corresponds to recallGroupMsg
 */
ErrorCode GroupMsgsRpcImp::recall(const std::string& sourceExtra,
                                  const std::string& uid,
                                  uint64_t gid,
                                  uint64_t mid,
                                  uint64_t& newMid)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::RecallGroupMsgReq request;
    bcm::dao::rpc::RecallGroupMsgResp response;
    brpc::Controller cntl;

    request.set_sourceextra(sourceExtra);
    request.set_groupid(gid);
    request.set_uid(uid);
    request.set_mid(mid);

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.recall(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << " , req: " << request.Utf8DebugString();
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            newMid = response.newmid();
        }

        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what()
             << " , req: " << request.Utf8DebugString();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}



} // end namespace dao
} // end namespace bcm
