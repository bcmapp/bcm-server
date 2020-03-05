#pragma once

#include <vector>
#include <string>
#include <dao/client.h>
#include <proto/dao/stored_message.pb.h>
#include <proto/message/message_protocol.pb.h>

namespace bcm {

class MessagesManager {
public:
    MessagesManager();
    ~MessagesManager();

    bool store(const std::string& destination, uint32_t destinationDeviceId, uint32_t destinationRegistrationId,
               const Envelope& envelope, uint32_t& storedCount);
    bool get(const std::string& destination, uint32_t destinationDeviceId, uint32_t maxCount,
             std::vector<StoredMessage>& outMessages, bool& hasMore);
    bool clear(const std::string& destination, boost::optional<uint32_t> destinationDeviceId);
    bool del(const std::string& destination, uint64_t msgId);
    bool del(const std::string& destination, const std::vector<uint64_t>& msgIds);

    static Envelope convert(const StoredMessage& message);

private:
    std::shared_ptr<dao::StoredMessages> m_storedMessages{dao::ClientFactory::storedMessages()};
};

}
