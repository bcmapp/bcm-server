#include "limiters_rpc_impl.h"
#include "utils/log.h"

#include <map>

namespace bcm {
namespace dao {

LimitersRpcImpl::LimitersRpcImpl(brpc::Channel* ch)
        : stub(ch)
{

}

ErrorCode LimitersRpcImpl::getLimiters(const std::set<std::string>& keys, std::map<std::string, bcm::Limiter>& limiters)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetLimiterReq request;
    bcm::dao::rpc::GetLimiterResp response;
    brpc::Controller cntl;

    for (const auto& k : keys) {
        request.add_keys(k);
    }
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getLimiter(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            for (const auto& it : response.limiters()) {
                limiters.emplace(it.key(), it.limiter());
            }
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode LimitersRpcImpl::setLimiters(const std::map<std::string, bcm::Limiter>& limiters)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::SetLimiterReq request;
    bcm::dao::rpc::SetLimiterResp response;
    brpc::Controller cntl;

    for (const auto& it : limiters) {
        bcm::dao::rpc::LimiterInfo* p = request.add_limiters();
        if (p == nullptr) {
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        p->set_key(it.first);
        bcm::Limiter* l = p->mutable_limiter();
        if (!l) {
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        *l = it.second;
    }

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.setLimiter(&cntl, &request, &response, nullptr);
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

std::atomic<uint64_t> LimitersRpcImpl::logId(0);

}
}



