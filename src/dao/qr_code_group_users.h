#pragma once

#include <string>
#include "proto/brpc/rpc_qr_code_group_user.pb.h"
#include "proto/dao/error_code.pb.h"
#include "proto/dao/qr_code_group_user.pb.h"


namespace bcm {
namespace dao {

class QrCodeGroupUsers {
public:
    
    virtual ErrorCode set(const bcm::QrCodeGroupUser& user, int64_t ttl) = 0;

    virtual ErrorCode get(uint64_t gid, const std::string& uid, bcm::QrCodeGroupUser& user) = 0;

};

}  // namespace dao
}  // namespace bcm
