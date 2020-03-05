#pragma once

#include <brpc/channel.h>
#include "dao/opaque_data.h"
#include "proto/brpc/rpc_opaque_data.pb.h"

namespace bcm {
namespace dao {


class OpaqueDataRpcImpl : public OpaqueData {
public:
    OpaqueDataRpcImpl(brpc::Channel* ch);

    virtual ErrorCode setOpaque(
            const std::string& key,
            const std::string& value);

    virtual ErrorCode getOpaque(
            const std::string& key,
            std::string& value);

private:
    static std::atomic<uint64_t> logId;
    bcm::dao::rpc::OpaqueDataService_Stub stub;
};

} // namespace dao
} // namespace bcm

