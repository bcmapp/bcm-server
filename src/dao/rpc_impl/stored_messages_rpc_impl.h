#pragma once

#include <brpc/channel.h>
#include "dao/stored_messages.h"
#include "proto/brpc/rpc_stored_message.pb.h"
#include <memory>
#include <atomic>

namespace bcm {
namespace dao {


/*
 * StoredMessages dao virtual base class
 */
class StoredMessagesRpcImp : public StoredMessages{
public:
    StoredMessagesRpcImp(brpc::Channel* ch);

    virtual ErrorCode set(const bcm::StoredMessage& msg , uint32_t& unreadMsgCount);

    virtual ErrorCode get(const std::string& destination, uint32_t destinationDeviceId,
                          uint32_t maxCount, std::vector<bcm::StoredMessage>& msgs);

    virtual ErrorCode del(const std::string& destination, const std::vector<uint64_t>& msgId);

    virtual ErrorCode clear(const std::string& destination);

    virtual ErrorCode clear(const std::string& destination, uint32_t destinationDeviceId);

private:
    static std::atomic<uint64_t> logId;
    bcm::dao::rpc::MessageService_Stub stub;

};


}  // namespace dao
}  // namespace bcm