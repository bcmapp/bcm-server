#pragma  once
#include <brpc/channel.h>
#include <memory>
#include <atomic>
#include "dao/limiter_configurations.h"
#include "proto/brpc/rpc_kv_paris.pb.h"

namespace bcm {
namespace dao {

class LimiterConfigurationRpcImpl : public LimiterConfigurations {
public:
    LimiterConfigurationRpcImpl(brpc::Channel* ch);

    ErrorCode load(LimiterConfigs& configs) override;

    ErrorCode get(const std::set<std::string>& keys, LimiterConfigs& configs) override;

    ErrorCode set(const LimiterConfigs& configs) override;

private:
    static std::atomic<uint64_t> logId;
    bcm::dao::rpc::KVPairsService_Stub stub;
};

}
}
