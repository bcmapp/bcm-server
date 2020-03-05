#include "signup_challenges_rpc_impl.h"
#include "utils/log.h"

#include <map>

namespace bcm {
namespace dao {

SignupChallengesRpcImpl::SignupChallengesRpcImpl(brpc::Channel* ch)
    : stub(ch)
{

}

ErrorCode SignupChallengesRpcImpl::get(const std::string& uid, bcm::SignUpChallenge& challenge)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetSignUpChallengeReq request;
    bcm::dao::rpc::GetSignUpChallengeResp response;
    brpc::Controller cntl;

    request.set_uid(uid);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getSignUpChallenge(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            challenge = response.challenge();
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode SignupChallengesRpcImpl::set(const std::string& uid, const bcm::SignUpChallenge& challenge)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::SetSignUpChallengeReq request;
    bcm::dao::rpc::SetSignUpChallengeResp response;
    brpc::Controller cntl;

    request.set_uid(uid);
    bcm::SignUpChallenge *p = request.mutable_challenge();
    if (p == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
    *p = challenge;

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.setSignUpChallenge(&cntl, &request, &response, nullptr);
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

ErrorCode SignupChallengesRpcImpl::del(const std::string& uid)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::DelSignUpChallengeReq request;
    bcm::dao::rpc::DelSignUpChallengeResp response;
    brpc::Controller cntl;

    request.set_uid(uid);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.delSignUpChallenge(&cntl, &request, &response, nullptr);
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

std::atomic<uint64_t> SignupChallengesRpcImpl::logId(0);

}
}
