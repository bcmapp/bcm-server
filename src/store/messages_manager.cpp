#include "messages_manager.h"
#include <utils/log.h>

namespace bcm {

MessagesManager::MessagesManager() = default;
MessagesManager::~MessagesManager() = default;

bool MessagesManager::store(const std::string& destination, uint32_t destinationDeviceId,
                            uint32_t destinationRegistrationId, const Envelope& envelope,
                            uint32_t& storedCount)
{
    StoredMessage message;
    message.set_msgtype(static_cast<StoredMessage::MsgType>(envelope.type()));

    message.set_destination(destination);
    message.set_destinationdeviceid(destinationDeviceId);
    message.set_destinationregistrationid(destinationRegistrationId);

    message.set_source(envelope.source());
    message.set_sourcedeviceid(envelope.sourcedevice());
    message.set_sourceregistrationid(envelope.sourceregistration());
    message.set_sourceextra(envelope.sourceextra());

    message.set_relay(envelope.relay());
    message.set_timestamp(envelope.timestamp());
    message.set_content(envelope.content());

    auto error = m_storedMessages->set(message, storedCount);
    if (error != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to insert message to db, error: " << error
             << ", destination: " << destination << "." << destinationDeviceId
             << ", source: " << envelope.sourcedevice() << "." << envelope.sourcedevice();
        return false;
    }
    return true;
}

bool MessagesManager::get(const std::string& destination, uint32_t destinationDeviceId, uint32_t maxCount,
                          std::vector<StoredMessage>& outMessages, bool& hasMore)
{
    hasMore = false;

    auto error = m_storedMessages->get(destination, destinationDeviceId, maxCount, outMessages);
    if (error == dao::ERRORCODE_NO_SUCH_DATA) {
        return true;
    }

    if (error != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to read offline message from db, error: " << error
             << ", destination: " << destination << "." << destinationDeviceId;
        return false;
    }

    hasMore = (outMessages.size() >= maxCount);
    return true;
}

bool MessagesManager::clear(const std::string& destination, boost::optional<uint32_t> destinationDeviceId)
{
    bool bSpecifyDevice = (destinationDeviceId != boost::none);
    dao::ErrorCode error;
    if (bSpecifyDevice) {
        error = m_storedMessages->clear(destination, *destinationDeviceId);
    } else {
        error = m_storedMessages->clear(destination);
    }

    if (error != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to clear message db, error: " << error
             << ", destination: " << destination << "." << (bSpecifyDevice ? *destinationDeviceId : 0);
        return false;
    }
    return true;
}

bool MessagesManager::del(const std::string& destination, uint64_t msgId)
{
    return del(destination, std::vector<uint64_t>(1, msgId));
}

bool MessagesManager::del(const std::string& destination, const std::vector<uint64_t>& msgIds)
{
    auto error = m_storedMessages->del(destination, msgIds);
    if (error != dao::ERRORCODE_SUCCESS) {
        LOGE << "failed to delete message from db, error: " << error
             << ", destination: " << destination;
        return false;
    }

    return true;
}

Envelope MessagesManager::convert(const StoredMessage& message)
{
    Envelope envelope;
    envelope.set_type(static_cast<Envelope::Type>(message.msgtype()));
    envelope.set_timestamp(message.timestamp());
    envelope.set_source(message.source());
    envelope.set_sourcedevice(message.sourcedeviceid());
    envelope.set_sourceregistration(message.sourceregistrationid());
    envelope.set_sourceextra(message.sourceextra());

    if (!message.relay().empty()) {
        envelope.set_relay(message.relay());
    }
    if (!message.content().empty()) {
        envelope.set_content(message.content());
    }

    return envelope;
}

}
