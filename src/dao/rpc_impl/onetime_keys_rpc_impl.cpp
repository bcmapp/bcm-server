#include "onetime_keys_rpc_impl.h"
#include "utils/log.h"
#include "../../proto/brpc/rpc_onetime_key.pb.h"

#include <map>

namespace bcm {
namespace dao {

OnetimeKeysRpcImpl::OnetimeKeysRpcImpl(brpc::Channel* ch)
    : stub(ch)
{

}

ErrorCode OnetimeKeysRpcImpl::get(const std::string& uid, uint32_t deviceId, bcm::OnetimeKey& key)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetOneTimeKeyByDeviceReq request;
    bcm::dao::rpc::GetOneTimeKeyByDeviceResp response;
    brpc::Controller cntl;

    request.set_uid(uid);
    request.set_deviceid(deviceId);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getOneTimeKeyByDevice(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            key = response.key();
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode OnetimeKeysRpcImpl::get(const std::string& uid, std::vector <bcm::OnetimeKey>& pKeys)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetOneTimeKeyByUserReq request;
    bcm::dao::rpc::GetOneTimeKeyByUserResp response;
    brpc::Controller cntl;

    request.set_uid(uid);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getOneTimeKeyByUser(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            pKeys.clear();
            pKeys.assign(response.keys().begin(), response.keys().end());
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

}

ErrorCode OnetimeKeysRpcImpl::set(const std::string& uid, uint32_t deviceId,
                                  const std::vector <bcm::OnetimeKey>& keys,
                                  const std::string& identityKey,
                                  const bcm::SignedPreKey& signedPreKey)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::SetOneTimeKeyReq request;
    bcm::dao::rpc::SetOneTimeKeyResp response;
    brpc::Controller cntl;

    request.set_uid(uid);
    request.set_deviceid(deviceId);
    request.set_identitykey(identityKey);
    bcm::SignedPreKey* signedKeyPtr = request.mutable_signedprekey();
    *signedKeyPtr = signedPreKey;

    for (const auto& k : keys) {
        bcm::OnetimeKey *p = request.add_keys();
        if (p == nullptr) {
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        *p = k;
    }
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.setOneTimeKey(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

}

ErrorCode OnetimeKeysRpcImpl::getCount(const std::string& uid, uint32_t deviceId, uint32_t& count)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetOneTimeKeyCountReq request;
    bcm::dao::rpc::GetOneTimeKeyCountResp response;
    brpc::Controller cntl;

    request.set_uid(uid);
    request.set_deviceid(deviceId);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getOneTimeKeyCount(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            count = response.count();
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

}

/*
* return ERRORCODE_SUCCESS if clear onetimeKeys success
*/
ErrorCode OnetimeKeysRpcImpl::clear(const std::string& uid)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
    bcm::dao::rpc::ClearOneTimeKeyReq request;
    bcm::dao::rpc::ClearOneTimeKeyResp response;

    brpc::Controller cntl;

    request.set_type(bcm::dao::rpc::ClearOneTimeKeyReq_ClearType_CLEAR_BY_UID);
    request.set_uid(uid);

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.clearOneTypeKey(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "OnetimeKeysRpcImpl::clear uid: " << uid
                 << " ,ErrorCode: " << cntl.ErrorCode()
                 << " ,ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        return ec;
    } catch (const std::exception& ex) {
        LOGE << "OnetimeKeysRpcImpl::clear uid: " << uid
             << " ,ErrorCode: " << cntl.ErrorCode()
             << " ,ErrorText: " << berror(cntl.ErrorCode())
             << " ,what: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

/*
 * return ERRORCODE_SUCCESS if clear onetimeKeys success
 */
ErrorCode OnetimeKeysRpcImpl::clear(const std::string& uid, uint32_t deviceId)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::ClearOneTimeKeyReq request;
    bcm::dao::rpc::ClearOneTimeKeyResp response;

    brpc::Controller cntl;

    request.set_type(bcm::dao::rpc::ClearOneTimeKeyReq_ClearType_CLEAR_BY_DEVICE);
    request.set_uid(uid);
    request.set_deviceid(deviceId);

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.clearOneTypeKey(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "OnetimeKeysRpcImpl::clear uid: " << uid
                 << " ,deviceId: " << deviceId
                 << " ,ErrorCode: " << cntl.ErrorCode()
                 << " ,ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        return ec;
    } catch (const std::exception& ex) {
        LOGE << "OnetimeKeysRpcImpl::clear uid: " << uid
             << " ,deviceId: " << deviceId
             << " ,ErrorCode: " << cntl.ErrorCode()
             << " ,ErrorText: " << berror(cntl.ErrorCode())
             << " ,what: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}


std::atomic<uint64_t> OnetimeKeysRpcImpl::logId(0);

}
}

