#pragma once

#include <string>
#include "proto/dao/sign_up_challenge.pb.h"
#include "proto/dao/error_code.pb.h"

namespace bcm {
namespace dao {

/*
 * SignUpChallenges dao virtual base class
 */
class SignUpChallenges {
public:
    /*
     * return ERRORCODE_SUCCESS if get SignUpChallenge success
     */
    virtual ErrorCode get(const std::string& uid, bcm::SignUpChallenge& challenge) = 0;

    /*
     * return ERRORCODE_SUCCESS if set SignUpChallenge success
     */
    virtual ErrorCode set(const std::string& uid, const bcm::SignUpChallenge& challenge) = 0;

    /*
     * return ERRORCODE_SUCCESS if delete SignUpChallenge success
     */
    virtual ErrorCode del(const std::string& uid) = 0;
};

}  // namespace dao
}  // namespace bcm
