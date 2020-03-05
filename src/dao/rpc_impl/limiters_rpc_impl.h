#pragma once

#include <brpc/channel.h>
#include <atomic>
#include "dao/limiters.h"
#include "proto/brpc/rpc_limiters.pb.h"

namespace bcm {
namespace dao {

class LimitersRpcImpl : public Limiters{
public:
    LimitersRpcImpl(brpc::Channel* ch);

    virtual ErrorCode getLimiters(const std::set<std::string>& keys, std::map<std::string, bcm::Limiter>& limiters);

    virtual ErrorCode setLimiters(const std::map<std::string, bcm::Limiter>& limiters);

private:
    static std::atomic<uint64_t> logId;
    bcm::dao::rpc::LimiterService_Stub stub;

};

} // namespace dao
}  // namespace bcm

