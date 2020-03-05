#pragma once

#include <memory>

#include <brpc/channel.h>
#include <atomic>
#include "dao/sign_up_challenges.h"
#include "proto/brpc/rpc_sign_up_challenge.pb.h"

namespace bcm {
namespace dao {

class SignupChallengesRpcImpl : public SignUpChallenges {
public:
    SignupChallengesRpcImpl(brpc::Channel* ch);

    virtual ErrorCode get(const std::string& uid, bcm::SignUpChallenge& challenge) override;

    virtual ErrorCode set(const std::string& uid, const bcm::SignUpChallenge& challenge) override;

    virtual ErrorCode del(const std::string& uid) override;

private:
    static std::atomic<uint64_t> logId;
    bcm::dao::rpc::SignUpChallengeService_Stub stub;
};

}
}
