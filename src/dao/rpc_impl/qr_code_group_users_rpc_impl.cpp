#include "qr_code_group_users_rpc_impl.h"
#include "utils/log.h"

namespace bcm {
namespace dao {

QrCodeGroupUsersRpcImpl::QrCodeGroupUsersRpcImpl(brpc::Channel* ch)
    : stub(ch)
{

}

ErrorCode QrCodeGroupUsersRpcImpl::get(uint64_t gid, const std::string& uid, bcm::QrCodeGroupUser& user)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetQrCodeGroupUserReq request;
    bcm::dao::rpc::GetQrCodeGroupUserResp response;
    brpc::Controller cntl;

    request.set_gid(gid);
    request.set_uid(uid);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));

    try {
        stub.getQrCodeGroupUser(&cntl, &request, &response, nullptr);
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

ErrorCode QrCodeGroupUsersRpcImpl::set(const bcm::QrCodeGroupUser& user, int64_t ttl)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::SetQrCodeGroupUserReq request;
    bcm::dao::rpc::SetQrCodeGroupUserResp response;
    brpc::Controller cntl;

    request.set_ttl(ttl);
    bcm::QrCodeGroupUser* p = request.mutable_user();
    *p = user;
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));

    try {
        stub.setQrCodeGroupUser(&cntl, &request, &response, nullptr);
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

std::atomic<uint64_t> QrCodeGroupUsersRpcImpl::logId(0);

}
}
