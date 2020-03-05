#pragma once

#include <brpc/channel.h>
#include "dao/accounts.h"
#include "proto/brpc/rpc_account.pb.h"
#include <memory>
#include <atomic>

namespace bcm {
namespace dao {


/*
 * Accounts dao virtual base class
 */
class AccountsRpcImp : public Accounts {
public:
    AccountsRpcImp(brpc::Channel* ch);

    /*
     * return ERRORCODE_SUCCESS if create account success
     */
    virtual ErrorCode create(const bcm::Account& account);

    /*
     * return ERRORCODE_SUCCESS if update account success
     */
    virtual ErrorCode updateAccount(const bcm::Account& account, uint32_t flags);
    
    virtual ErrorCode updateAccount(const bcm::Account& account, const bcm::AccountField& modifyField);
    
    /*
     * return ERRORCODE_SUCCESS if update device success
     */
    virtual ErrorCode updateDevice(const bcm::Account& account, uint32_t deviceId);
    
    virtual ErrorCode updateDevice(const bcm::Account& account, const bcm::DeviceField& modifyField);
    
    /*
     * return ERRORCODE_SUCCESS if get account success, and fill account correctly
     */
    virtual ErrorCode get(const std::string& uid, bcm::Account& account);

    /*
     * return ERRORCODE_SUCCESS if get account success, and fill outputs correctly
     */
    virtual ErrorCode get(const std::vector<std::string>& uids,
                          std::vector<bcm::Account>& accounts,
                          std::vector<std::string>& missedUids);

    virtual ErrorCode getKeys(const std::set<std::string>& uids, std::vector<bcm::Keys>& keys);

    virtual ErrorCode getKeysByGid(uint64_t gid, std::vector<bcm::Keys>& keys);

private:
    bcm::dao::rpc::AccountService_Stub stub;
    static std::atomic<uint64_t> logId;
};

}  // namespace dao
}  // namespace bcm