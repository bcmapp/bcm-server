#include "pending_group_user_rpc_impl.h"
#include "utils/log.h"

#include <map>

namespace bcm {
namespace dao {

PendingGroupUserRpcImpl::PendingGroupUserRpcImpl(brpc::Channel* ch)
        : stub(ch)
{

}

ErrorCode PendingGroupUserRpcImpl::set(const PendingGroupUser& user)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::InsertPendingGroupUserReq request;
    bcm::dao::rpc::InsertPendingGroupUserResp response;
    brpc::Controller cntl;

    PendingGroupUser* ppgu = request.mutable_user();
    if (ppgu == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
    *ppgu = user;
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.insert(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

}

ErrorCode PendingGroupUserRpcImpl::query(uint64_t gid,
                                         const std::string& startUid,
                                         int count,
                                         std::vector<bcm::PendingGroupUser>& result)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::QueryPendingGroupUserReq request;
    bcm::dao::rpc::QueryPendingGroupUserResp response;
    brpc::Controller cntl;

    request.set_gid(gid);
    request.set_startuid(startUid);
    request.set_count(count);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.query(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            result.assign(response.users().begin(), response.users().end());
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

}

ErrorCode PendingGroupUserRpcImpl::del(uint64_t gid, std::set<std::string> uids)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::DelPendingGroupUserReq request;
    bcm::dao::rpc::DelPendingGroupUserResp response;
    brpc::Controller cntl;

    request.set_gid(gid);
    for (const auto& uid : uids) {
        request.add_uids(uid);
    }
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.del(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

}

ErrorCode PendingGroupUserRpcImpl::clear(uint64_t gid)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::ClearPendingGroupUserReq request;
    bcm::dao::rpc::ClearPendingGroupUserResp response;
    brpc::Controller cntl;

    request.set_gid(gid);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.clear(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

std::atomic<uint64_t> PendingGroupUserRpcImpl::logId(0);

}
}

