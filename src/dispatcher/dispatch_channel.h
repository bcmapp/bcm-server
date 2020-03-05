#pragma once

#include "idispatcher.h"
#include "dispatch_address.h"
#include "dispatch_manager.h"
#include "config/encrypt_sender.h"
#include <websocket/websocket_session.h>
#include <boost/optional.hpp>
#include <proto/contacts/friend.pb.h>
#include <proto/message/message_protocol.pb.h>
#include <proto/websocket/websocket_protocol.pb.h>
#include "proto/device/multi_device.pb.h"
#include <store/accounts_manager.h>

namespace bcm {

class DispatchChannel : public std::enable_shared_from_this<DispatchChannel>
                      , public IDispatcher {
public:
    DispatchChannel(asio::io_context& ioc, DispatchAddress address,
                    std::shared_ptr<WebsocketSession> wsClient,
                    std::weak_ptr<DispatchManager> dispatchManager,
                    EncryptSenderConfig& cfg);

    virtual ~DispatchChannel();

    uint64_t getIdentity() override;

    void onDispatchSubscribed() override;
    void onDispatchUnsubscribed(bool kicking) override;
    void onDispatchRedisMessage(const std::string& message) override;
    void onDispatchGroupMessage(const std::string& message) override;
    std::shared_ptr<WebsocketSession> getSession();
private:
    bool encrypt(const std::string& signalingKey, const std::string& plaintext, std::string& ciphertext);
    bool publishMessage(const DispatchAddress& address, const std::string& message);
    void dispatch(std::function<bool (WebsocketRequestMessage& request, std::vector<boost::any>& passing)> before,
                  std::function<bool (const WebsocketResponseMessage& response,
                                      const std::vector<boost::any>& passing)> after);

    void sendP2pMessage(const Envelope& envelope, boost::optional<uint64_t> storageId, bool remain);
    void sendStoredMessages();
    void sendStoredMessagesLegacy();
    bool sendReceipt(const Envelope& envelope, const std::string& payload);
    void sendEmpty();

    void sendFriendEventMessage(const FriendMessage& message);
    void sendStoredFriendEventMessages(const std::set<dao::FriendEventType>& types);

    bool isSupportDispatchBatch();
    bool isMessageStaleAndClientObsolete(const StoredMessage& message);
private:
    void sendMultiDeviceMessage(const MultiDeviceMessage& multiMsg);

private:
    bool m_bAvailable{false};
    asio::io_context& m_ioc;
    DispatchAddress m_address;
    std::shared_ptr<WebsocketSession> m_wsClient;
    std::weak_ptr<DispatchManager> m_dispatchManager;
    EncryptSenderConfig m_encryptSenderConfig;
};

}
