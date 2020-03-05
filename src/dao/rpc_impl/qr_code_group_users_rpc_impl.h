#pragma once
#include <brpc/channel.h>
#include <atomic>
#include "dao/qr_code_group_users.h"

namespace bcm {
namespace dao {

class QrCodeGroupUsersRpcImpl : public QrCodeGroupUsers {
public:
    QrCodeGroupUsersRpcImpl(brpc::Channel* ch);

    virtual ErrorCode set(const bcm::QrCodeGroupUser& user, int64_t ttl) override;

    virtual ErrorCode get(uint64_t gid, const std::string& uid, bcm::QrCodeGroupUser& user) override;

private:
    static std::atomic<uint64_t> logId;
    bcm::dao::rpc::QrCodeGroupUserService_Stub stub;
};

}
}
