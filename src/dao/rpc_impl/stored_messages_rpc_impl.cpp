//
// Created by shu wang on 2018/11/27.
//
#include <map>

#include "utils/log.h"

#include "stored_messages_rpc_impl.h"
#include "../../proto/brpc/rpc_stored_message.pb.h"


namespace bcm {
namespace dao {

std::atomic<uint64_t> StoredMessagesRpcImp::logId(0);

StoredMessagesRpcImp::StoredMessagesRpcImp(brpc::Channel* ch) : stub(ch)
{

}

ErrorCode StoredMessagesRpcImp::set(const bcm::StoredMessage& msg , uint32_t& unreadMsgCount)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::SetStoredMessageReq request;
    bcm::dao::rpc::SetStoredMessageResp response;
    brpc::Controller cntl;

    bcm::StoredMessage* newMsg = request.mutable_msg();
    *newMsg = msg;

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.set(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "StoredMessagesRpcImp set destination: " << msg.destination() << ",ErrorCode: "
                 << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            unreadMsgCount = response.unreadmsgcount();
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode StoredMessagesRpcImp::clear(const std::string& destination)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::ClearMessageByDestReq request;
    bcm::dao::rpc::ClearMessageByDestResp response;
    brpc::Controller cntl;

    request.set_destination(destination);

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.clearMessageByDest(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "StoredMessagesRpcImp clear destination: " << destination
                 << ",ErrorCode: "
                 << cntl.ErrorCode() << ",ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());

        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode StoredMessagesRpcImp::clear(const std::string& destination, uint32_t destinationDeviceId)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::ClearMessageByDeviceReq request;
    bcm::dao::rpc::ClearMessageByDeviceResp response;
    brpc::Controller cntl;

    request.set_destination(destination);
    request.set_destinationdeviceid(destinationDeviceId);

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.clearMessageByDevice(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "StoredMessagesRpcImp clear destination: " << destination
                << ",deviceId: " << destinationDeviceId << ",ErrorCode: "
                 << cntl.ErrorCode() << ",ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());

        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode StoredMessagesRpcImp::get(const std::string& destination, uint32_t destinationDeviceId,
                                    uint32_t maxCount, std::vector<bcm::StoredMessage>& msgs)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetStoredMessageReq request;
    bcm::dao::rpc::GetStoredMessageResp response;
    brpc::Controller cntl;

    request.set_destination(destination);
    request.set_destinationdeviceid(destinationDeviceId);
    request.set_limit(maxCount);

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.load(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "StoredMessagesRpcImp get destination: " << destination
                 << ", deviceId: " << destinationDeviceId << ",ErrorCode: "
                 << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            msgs.clear();
            msgs.assign(response.msgs().begin(), response.msgs().end());
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << "StoredMessagesRpcImp get destination: " << destination
             << ", deviceId: " << destinationDeviceId
             << ", maxCount: " << maxCount
             << ",Error what: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode StoredMessagesRpcImp::del(const std::string& destination, const std::vector<uint64_t>& msgId)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::DeleteMessageByIdReq request;
    bcm::dao::rpc::DeleteMessageByIdResp response;
    brpc::Controller cntl;

    request.set_destination(destination);

    for (const auto& itMsgId : msgId) {
        request.add_msg_ids(itMsgId);
    }

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.delMessageById(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "StoredMessagesRpcImp del destination: " << destination
                 << ",ErrorCode: " << cntl.ErrorCode()
                 << ",ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());

        return ec;
    } catch (const std::exception& ex) {
        LOGE << "StoredMessagesRpcImp del destination: " << destination
             << " ,Error what: " << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

}  // namespace dao
}  // namespace bcm
