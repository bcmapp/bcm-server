#include "utilities_rpc_impl.h"
#include "utils/log.h"

#include <sstream>

namespace bcm {
namespace dao {

std::atomic<uint64_t> MasterLeaseRpcImpl::logId(0);

MasterLeaseRpcImpl::MasterLeaseRpcImpl(brpc::Channel* ch) : stub(ch)
{
    // To use as a active/standby lease, each instance should have an unique lease_value;
    // we generate it via a random UUID for boost.
    boost::uuids::basic_random_generator<boost::mt19937> random_generator;
    m_uuid = random_generator();
}

// For example, there are two processes P1 and P2
// P1 - UUID1
// P2 - UUID2
//
// When issue getLease concurrently, only one process will success
// due to 'Key Value NOT Exist or Empty', and will the set the lease_value
// to its UUID. After had got lease, it should call getLease repeatedly
// to refresh lease_key's TTL setting.
ErrorCode MasterLeaseRpcImpl::getLease(
    const std::string& key, 
    uint32_t ttlMs)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetLeaseReq  request;
    bcm::dao::rpc::GetLeaseResp response;
    brpc::Controller cntl;

    std::stringstream ss;
    ss << m_uuid /*<< "-" << std::this_thread::get_id()*/;
    std::string local_lease_value = ss.str();

    request.set_key(key);
    request.set_value(local_lease_value);
    request.set_ttlms(ttlMs);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_acq_rel));
    try {
        stub.getLease(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << "getLease exception: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode MasterLeaseRpcImpl::renewLease(
    const std::string& key, 
    uint32_t ttlMs)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::RenewLeaseReq  request;
    bcm::dao::rpc::RenewLeaseResp response;
    brpc::Controller cntl;

    std::stringstream ss;
    ss << m_uuid /*<< "-" << std::this_thread::get_id()*/;
    std::string local_lease_value = ss.str();

    request.set_key(key);
    request.set_value(local_lease_value);
    request.set_ttlms(ttlMs);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_acq_rel));
    try {
        stub.renewLease(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << "renewLease exception: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

// When a process intends to release lease, it should be the right
// owner of the lease, and set the lease_value to "", such that other
// process can get it sometimes later.
// Issue release with CT_VALUE_MATCH_ANYWHERE and local UUID to 
// guarantee caller is the right lease owner!
ErrorCode MasterLeaseRpcImpl::releaseLease(
    const std::string& key)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::ReleaseLeaseReq  request;
    bcm::dao::rpc::ReleaseLeaseResp response;
    brpc::Controller cntl;

    std::stringstream ss;
    ss << m_uuid /*<< "-" << std::this_thread::get_id()*/;
    std::string local_lease_value = ss.str();    

    request.set_key(key);
    request.set_value(local_lease_value);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_acq_rel));
    try {
        stub.releaseLease(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << "releaeLease exception: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

}
}



