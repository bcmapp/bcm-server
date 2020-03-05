#pragma once

#include <brpc/channel.h>
#include <atomic>
#include "../onetime_keys.h"
#include "../../proto/brpc/rpc_onetime_key.pb.h"

namespace bcm {
namespace dao {

/*
 * OnetimeKeys dao virtual base class
 */
class OnetimeKeysRpcImpl : public OnetimeKeys {
public:
    OnetimeKeysRpcImpl(brpc::Channel* ch);

    virtual ErrorCode get(const std::string& uid, std::vector <bcm::OnetimeKey>& key);

    virtual ErrorCode get(const std::string& uid, uint32_t deviceId, bcm::OnetimeKey& key);

    virtual ErrorCode getCount(const std::string& uid, uint32_t deviceId, uint32_t& count);

    virtual ErrorCode set(const std::string& uid, uint32_t deviceId,
                          const std::vector <bcm::OnetimeKey>& keys,
                          const std::string& identityKey,
                          const bcm::SignedPreKey& signedPreKey);

    /*
     * return ERRORCODE_SUCCESS if clear onetimeKeys success
     */
    virtual ErrorCode clear(const std::string& uid);

    /*
     * return ERRORCODE_SUCCESS if clear onetimeKeys success
     */
    virtual ErrorCode clear(const std::string& uid, uint32_t deviceId);

private:
    static std::atomic<uint64_t> logId;
    bcm::dao::rpc::OnetimeKeyService_Stub stub;

};

} // namespace dao
}  // namespace bcm
