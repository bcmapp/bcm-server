#include "opaque_data_rpc_impl.h"
#include "utils/log.h"

namespace bcm {
namespace dao {

std::atomic<uint64_t> OpaqueDataRpcImpl::logId(0);

OpaqueDataRpcImpl::OpaqueDataRpcImpl(brpc::Channel* ch)
    : stub(ch)
{
}

ErrorCode OpaqueDataRpcImpl::setOpaque(
        const std::string& key,
        const std::string& value)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::SetOpaqueReq request;
    bcm::dao::rpc::SetOpaqueResp response;
    brpc::Controller cntl;

    request.set_key(key);
    request.set_value(value);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_acq_rel));
    try {
        stub.setOpaque(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << "setOpaque exception: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode OpaqueDataRpcImpl::getOpaque(
        const std::string& key,
        std::string& value)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetOpaqueReq request;
    bcm::dao::rpc::GetOpaqueResp response;
    brpc::Controller cntl;

    request.set_key(key);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_acq_rel));
    try {
        stub.getOpaque(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        value = response.value();
        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << "setOpaque exception: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

}
}



