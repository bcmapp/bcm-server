#pragma once
#include <brpc/channel.h>
#include <atomic>
#include "../pending_group_users.h"
#include "../../proto/brpc/rpc_pending_group_user.pb.h"


namespace bcm {
namespace dao {

class PendingGroupUserRpcImpl : public PendingGroupUsers {
public:
    PendingGroupUserRpcImpl(brpc::Channel* ch);

    virtual ErrorCode set(const PendingGroupUser& user) override;

    virtual ErrorCode query(uint64_t gid,
                            const std::string& startUid,
                            int count,
                            std::vector<PendingGroupUser>& result) override;

    virtual ErrorCode del(uint64_t gid, std::set<std::string> uids) override;

    virtual ErrorCode clear(uint64_t gid) override;
private:
    static std::atomic<uint64_t> logId;
    bcm::dao::rpc::PendingGroupUserService_Stub stub;

};

} // namespace dao
}  // namespace bcm
