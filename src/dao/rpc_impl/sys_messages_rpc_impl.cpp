//
// Created by shu wang on 2019/3/22.
//
#include <map>

#include "utils/log.h"

#include "sys_messages_rpc_impl.h"


namespace bcm {
namespace dao {

std::atomic<uint64_t> SysMsgsRpcImpl::logId(0);

SysMsgsRpcImpl::SysMsgsRpcImpl(brpc::Channel* ch) : stub(ch)
{

}

ErrorCode SysMsgsRpcImpl::get(
    const std::string& destination
    , std::vector<bcm::SysMsg>& msgs
    , uint32_t maxMsgSize)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetSysMessageInfoReq request;
    bcm::dao::rpc::GetSysMessageInfoResp response;
    brpc::Controller cntl;

    request.set_destination(destination);
    request.set_limit(maxMsgSize);

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.get(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "SysMsgsRpcImpl get destination: " << destination
                 << ", maxMsgSize: " << maxMsgSize
                 << ", ErrorCode: " << cntl.ErrorCode()
                 << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            msgs.clear();
            msgs.assign(response.msgs().begin(), response.msgs().end());
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << "SysMsgsRpcImpl get destination: " << destination
             << ", maxMsgSize: " << maxMsgSize
             << ", what: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

/*
 * return ERRORCODE_SUCCESS if del message success
 * @Deprecated
 */
ErrorCode SysMsgsRpcImpl::del(
    const std::string& destination
    , uint64_t msgId)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::BatchDeleteSysMessageReq request;
    bcm::dao::rpc::BatchDeleteSysMessageResp response;
    brpc::Controller cntl;

    request.set_destination(destination);
    request.add_msg_ids(msgId);

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.batchDelete(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "SysMsgsRpcImpl del destination: " << destination
                 << ", msgId: " << msgId
                 << ", ErrorCode: " << cntl.ErrorCode()
                 << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << "SysMsgsRpcImpl del destination: " << destination
             << ", msgId: " << msgId
             << ", what: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

/*
 * return ERRORCODE_SUCCESS if del message success
 */
ErrorCode SysMsgsRpcImpl::delBatch(
    const std::string& destination
    , const std::vector<uint64_t>& msgIds)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::BatchDeleteSysMessageReq request;
    bcm::dao::rpc::BatchDeleteSysMessageResp response;
    brpc::Controller cntl;

    request.set_destination(destination);
    for (const auto& itMsg : msgIds) {
        request.add_msg_ids(itMsg);
    }

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.batchDelete(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "SysMsgsRpcImpl delBatch destination: " << destination
                 << ", ErrorCode: " << cntl.ErrorCode()
                 << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << "SysMsgsRpcImpl delBatch destination: " << destination
             << ", what: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

/*
 * return ERRORCODE_SUCCESS if clear msgs success
 * del all msgs for destination that is equal or less than maxMid
 */
ErrorCode SysMsgsRpcImpl::delBatch(const std::string& destination, uint64_t maxMid)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::CleanSysMessageReq request;
    bcm::dao::rpc::CleanSysMessageResp response;
    brpc::Controller cntl;

    request.set_destination(destination);
    request.set_maxmid(maxMid);

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.cleanSysMsg(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "SysMsgsRpcImpl delBatch destination: " << destination
                 << ", maxMid: " << maxMid
                 << ", ErrorCode: " << cntl.ErrorCode()
                 << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << "SysMsgsRpcImpl delBatch destination: " << destination
             << ", maxMid: " << maxMid
             << ", what: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

/*
 * return ERRORCODE_SUCCESS if insert message success
 */
ErrorCode SysMsgsRpcImpl::insert(const bcm::SysMsg& msg)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::InsertSysMessageReq request;
    bcm::dao::rpc::InsertSysMessageResp response;
    brpc::Controller cntl;

    ::bcm::SysMsg* newMsg = request.add_sys_msg();
    *newMsg = msg;

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.insert(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "SysMsgsRpcImpl insert destination: " << msg.destination()
                 << ", msgid: " << msg.sysmsgid()
                 << ", ErrorCode: " << cntl.ErrorCode()
                 << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << "SysMsgsRpcImpl insert destination: " << msg.destination()
             << ", msgid: " << msg.sysmsgid()
             << ", what: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode SysMsgsRpcImpl::insertBatch(const  std::vector<bcm::SysMsg>& msgs)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::InsertSysMessageReq request;
    bcm::dao::rpc::InsertSysMessageResp response;
    brpc::Controller cntl;

    for (const auto& itMsg : msgs) {
        ::bcm::SysMsg* newMsg = request.add_sys_msg();
        if (newMsg == nullptr) {
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        *newMsg = itMsg;
    }

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.insert(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "SysMsgsRpcImpl insertBatch "
                 << ", ErrorCode: " << cntl.ErrorCode()
                 << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        return ec;
    } catch (const std::exception& ex) {
        LOGE << "SysMsgsRpcImpl insertBatch "
             << ", what: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}




}  // namespace dao
}  // namespace bcm
