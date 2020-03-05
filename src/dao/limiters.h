#pragma once

#include <proto/dao/limiter.pb.h>
#include "proto/dao/error_code.pb.h"

namespace bcm {
namespace dao {

class Limiters {
public:
   /*
    * return ERRORCODE_SUCCESS if get VerificationCodeLimiter success
    */
    virtual ErrorCode getLimiters(
            const std::set<std::string>& keys,
            std::map<std::string, bcm::Limiter>& limiters) = 0;

    /*
     * return ERRORCODE_SUCCESS if set VerificationCodeLimiter success
     */
    virtual ErrorCode setLimiters(
            const std::map<std::string, bcm::Limiter>& limiters) = 0;
};

}
}
