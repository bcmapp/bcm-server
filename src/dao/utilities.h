#pragma once

#include "proto/dao/error_code.pb.h"

namespace bcm {
namespace dao {

class MasterLease {
public:
   /*
    * Try to get lease, the lease will be successfully obtained only if 
    * the record does not exist or if the record is empty
    *
    * return ERRORCODE_SUCCESS if get lease success
    */
    virtual ErrorCode getLease(
            const std::string& key,
            uint32_t ttlMs) = 0;

   /*
    * Try to renew lease, renewing will be successfully only if 
    * the caller is the current lease holder
    *
    * return ERRORCODE_SUCCESS if get lease success
    */
    virtual ErrorCode renewLease(
            const std::string& key,
            uint32_t ttlMs) = 0;            

    /*
     * Try to release lease, the lease will be successfully released
     * only if caller is the current lease holder
     * 
     * return ERRORCODE_SUCCESS if set VerificationCodeLimiter success
     */
    virtual ErrorCode releaseLease(
            const std::string& key) = 0;
};

}
}
