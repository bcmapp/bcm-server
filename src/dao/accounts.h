#pragma once

#include <string>
#include "proto/dao/account.pb.h"
#include "proto/dao/device.pb.h"
#include "proto/dao/error_code.pb.h"

namespace bcm {
namespace dao {

/*
 * Accounts dao virtual base class
 */
class Accounts {
public:
    /*
     * return ERRORCODE_SUCCESS if create account success
     */
    virtual ErrorCode create(const bcm::Account& account) = 0;

    /*
     * return ERRORCODE_SUCCESS if update account success
     */
    virtual ErrorCode updateAccount(const bcm::Account& account, uint32_t flags) = 0;
    
    virtual ErrorCode updateAccount(const bcm::Account& account, const bcm::AccountField& modifyField) = 0;

    /*
     * return ERRORCODE_SUCCESS if update device success
     */
    virtual ErrorCode updateDevice(const bcm::Account& account, uint32_t deviceId) = 0;

    virtual ErrorCode updateDevice(const bcm::Account& account, const bcm::DeviceField& modifyField) = 0;

    /*
     * return ERRORCODE_SUCCESS if get account success, and fill account correctly
     */
    virtual ErrorCode get(const std::string& uid, bcm::Account& account) = 0;

    /*
     * return ERRORCODE_SUCCESS if get account success, and fill outputs correctly
     */
    virtual ErrorCode get(
            const std::vector<std::string>& uids
            , std::vector<bcm::Account>& accounts
            , std::vector<std::string>& missedUids) = 0;

    virtual ErrorCode getKeys(const std::set<std::string>& uids, std::vector<bcm::Keys>& keys) = 0;

    virtual ErrorCode getKeysByGid(uint64_t gid, std::vector<bcm::Keys>& keys) = 0;
};

}  // namespace dao
}  // namespace bcm
