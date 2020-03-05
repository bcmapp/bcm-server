//
// Created by shu wang on 2018/11/19.
//

#include <map>

#include "utils/log.h"

#include "accounts_rpc_impl.h"
#include "../../proto/brpc/rpc_account.pb.h"
#include "../../proto/dao/device.pb.h"
#include "../../proto/dao/account.pb.h"

namespace bcm {
namespace dao {

std::atomic<uint64_t> AccountsRpcImp::logId(0);

AccountsRpcImp::AccountsRpcImp(brpc::Channel* ch) : stub(ch)
{

}

/*
 * return ERRORCODE_SUCCESS if create account success
 */
ErrorCode AccountsRpcImp::create(const bcm::Account& account)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::CreateAccountReq request;
    bcm::dao::rpc::CreateAccountResp response;
    brpc::Controller cntl;

    ::bcm::Account* newAccount = request.mutable_acc();
    *newAccount = account;

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.create(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "account create uid: " << account.uid() << ",ErrorCode: "
                 << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

/*
 * return ERRORCODE_SUCCESS if update account success
 */
ErrorCode AccountsRpcImp::updateAccount(const bcm::Account& account, uint32_t flags)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::UpdateAccountReq request;
    bcm::dao::rpc::UpdateAccountResp response;
    brpc::Controller cntl;

    request.set_flags(flags);
    ::bcm::Account* newAccount = request.mutable_acc();
    *newAccount = account;

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.updateAccount(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "updateAccount uid: " << account.uid() << ",ErrorCode: "
                 << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

/*
 * return ERRORCODE_SUCCESS if update device success
 */
ErrorCode AccountsRpcImp::updateDevice(const bcm::Account& account, uint32_t deviceId)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::UpdateAccountDeviceReq request;
    bcm::dao::rpc::UpdateAccountDeviceResp response;
    brpc::Controller cntl;

    request.set_deviceid(deviceId);
    ::bcm::Account* newAccount = request.mutable_acc();
    *newAccount = account;

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.updateDevice(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "update account device uid: " << account.uid() << ",ErrorCode: "
                 << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode AccountsRpcImp::updateAccount(const bcm::Account& account, const bcm::AccountField& modifyField)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
    
    bcm::dao::rpc::UpdateAccountReq request;
    bcm::dao::rpc::UpdateAccountResp response;
    brpc::Controller cntl;
    
    ::bcm::Account* newAccount = request.mutable_acc();
    *newAccount = account;
    ::bcm::AccountField* newModifyField = request.mutable_updatefield();
    *newModifyField = modifyField;
    
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.updateAccount(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "updateAccount uid: " << account.uid() << ", ErrorCode: "
                 << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        return ec;
    } catch (const std::exception& ex) {
        LOGE << "updateAccount uid: " << account.uid() << ", what: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode AccountsRpcImp::updateDevice(const bcm::Account& account, const bcm::DeviceField& modifyField)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
    
    bcm::dao::rpc::UpdateAccountDeviceReq request;
    bcm::dao::rpc::UpdateAccountDeviceResp response;
    brpc::Controller cntl;
    
    request.set_deviceid(modifyField.id());
    ::bcm::Account* newAccount = request.mutable_acc();
    *newAccount = account;
    ::bcm::DeviceField* newModifyDevice = request.mutable_updatedevicefield();
    *newModifyDevice = modifyField;
    
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.updateDevice(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "update account device uid: " << account.uid() << ",ErrorCode: "
                 << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        return ec;
    } catch (const std::exception& ex) {
        LOGE << "update account device uid: " << account.uid() << ", what: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

/*
 * return ERRORCODE_SUCCESS if get account success, and fill account correctly
 */
ErrorCode AccountsRpcImp::get(const std::string& uid, bcm::Account& account)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetAccountReq request;
    bcm::dao::rpc::GetAccountResp response;
    brpc::Controller cntl;

    request.set_uid(uid);

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getAccount(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "get account uid: " << uid << ",ErrorCode: "
                 << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            account = response.acc();
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

}

ErrorCode AccountsRpcImp::get(const std::vector<std::string>& uids,
                              std::vector<bcm::Account>& accounts,
                              std::vector<std::string>& missedUids)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetMultiAccountsReq request;
    bcm::dao::rpc::GetMultiAccountsResp response;
    brpc::Controller cntl;

    for (const auto& uid : uids) {
        request.add_uids(uid);
    }

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getMultiAccounts(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << ", uid count: " << uids.size()
                 << ", uids: " << toString(uids) ;
            
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            std::set<std::string> missed(uids.begin(), uids.end());
            for (const auto& ac : response.accounts()) {
                accounts.emplace_back(ac);
                missed.erase(ac.uid());
            }
            missedUids.clear();
            if (!missed.empty()) {
                missedUids.assign(missed.begin(), missed.end());
            }
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode AccountsRpcImp::getKeys(const std::set<std::string>& uids, std::vector<bcm::Keys>& keys)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetKeysReq request;
    bcm::dao::rpc::GetKeysResp response;
    brpc::Controller cntl;

    for (const auto& uid : uids) {
        request.add_uids(uid);
    }

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getKeys(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << ", uid count: " << uids.size()
                 << ", uids: " << toString(uids) ;
            
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            keys.assign(response.keys().begin(), response.keys().end());
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode AccountsRpcImp::getKeysByGid(uint64_t gid, std::vector<bcm::Keys>& keys)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetKeysByGidReq request;
    bcm::dao::rpc::GetKeysByGidResp response;
    brpc::Controller cntl;
    request.set_gid(gid);

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getKeysByGid(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << ", gid: " << gid;
            
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            keys.assign(response.keys().begin(), response.keys().end());
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}


}  // namespace dao
}  // namespace bcm