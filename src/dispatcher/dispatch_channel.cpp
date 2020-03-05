#include "dispatch_channel.h"
#include <proto/websocket/websocket_protocol.pb.h>
#include <proto/dao/stored_message.pb.h>
#include <metrics_client.h>
#include <crypto/base64.h>
#include <crypto/evp_cipher.h>
#include <crypto/hmac.h>
#include <utils/time.h>
#include <utils/log.h>
#include <utils/account_helper.h>
#include <push/push_service.h>
#include <utils/sender_utils.h>
#include "proto/contacts/friend.pb.h"
#include "proto/device/multi_device.pb.h"
#include <redis/hiredis_client.h>
#include "redis/reply.h"
#include "redis/online_redis_manager.h"
#include "store/accounts_manager.h"

namespace bcm {

using namespace metrics;

static constexpr char kMetricsWebsocketServiceName[] = "websocket";
static constexpr uint32_t kMaxCountDispatchOnce = 50;

DispatchChannel::DispatchChannel(asio::io_context& ioc, DispatchAddress address,
                                 std::shared_ptr<WebsocketSession> wsClient,
                                 std::weak_ptr<DispatchManager> dispatchManager,
                                 EncryptSenderConfig& cfg)
    : m_ioc(ioc)
    , m_address(std::move(address))
    , m_wsClient(std::move(wsClient))
    , m_dispatchManager(std::move(dispatchManager))
    , m_encryptSenderConfig(cfg)
{
}

DispatchChannel::~DispatchChannel() = default;

uint64_t DispatchChannel::getIdentity()
{
    return reinterpret_cast<uint64_t>(this);
}

void DispatchChannel::onDispatchSubscribed()
{
    LOGI << "success to subscribe response.(" << m_address << "), available: " << m_bAvailable;
    if (m_bAvailable) {
        return;
    }
    m_bAvailable = true;

    if (m_wsClient->getAuthType() == WebsocketService::REQUESTID_AUTH) {
        return;
    }

    sendStoredMessages();

    // slave device dont send friend msg, for now
    if (m_address.getDeviceid() != Device::MASTER_ID) {
        return;
    }


    std::set<dao::FriendEventType> types;
    types.insert(dao::FriendEventType::FRIEND_REQUEST);
    types.insert(dao::FriendEventType::FRIEND_REPLY);
    types.insert(dao::FriendEventType::DELETE_FRIEND);
    sendStoredFriendEventMessages(types);
}

void DispatchChannel::onDispatchUnsubscribed(bool kicking)
{
    LOGI << "success to unsubscribe response.(" << m_address << " kick " << kicking << ", m_bAvailable: " << m_bAvailable << ")";
    m_bAvailable = false;
    if (kicking) {
        LOGI << "disconnect websocket connection.(" << m_address << ")";
        m_wsClient->disconnect();
    }
}

void DispatchChannel::onDispatchRedisMessage(const std::string& message)
{
    if (!m_bAvailable) {
        return;
    }

    PubSubMessage pubMessage;
    if (!pubMessage.ParseFromString(message)) {
        LOGE << "failed to parse pubsub redis message.(" << m_address << ")";
        return;
    }

    LOGD << "success to parse pubsub redis message.(" << m_address << " " << message.size() << ")";

    switch (pubMessage.type()) {
        case PubSubMessage::QUERY_DB:
            sendStoredMessages();
            break;
        case PubSubMessage::DELIVER: {
            Envelope envelope;
            if (!envelope.ParseFromString(pubMessage.content())) {
                LOGE << "failed to parse im message.(" << m_address << ")";
                return;
            }
            sendP2pMessage(envelope, boost::none, false);
            break;
        }
        case PubSubMessage::CONNECTED:
            if (!pubMessage.content().empty() && pubMessage.content() != std::to_string(getIdentity())) {
                LOGE << "a new connection coming on another server for " << m_address;
                m_bAvailable = false;
                m_wsClient->disconnect();
            }
            break;
        case PubSubMessage::MULTI_DEVICE: {
            MultiDeviceMessage msg;
            if (!msg.ParseFromString(pubMessage.content())) {
                LOGE << "failed to parse multi_device message.(" << m_address << ")";
                return;
            }
            sendMultiDeviceMessage(msg);
            break;
        }
        case PubSubMessage::CLOSE:
        case PubSubMessage::KEEPALIVE:
        case PubSubMessage::CHECK:
        case PubSubMessage::QUERY_ONLINE:
            break;
        case PubSubMessage::FRIEND: {
            FriendMessage friendMsg;
            if (!friendMsg.ParseFromString(pubMessage.content())) {
                LOGE << "failed to parse friend message.(" << m_address << ")";
                return;
            }
            sendFriendEventMessage(friendMsg);
            break;
        }
        case PubSubMessage::NOTIFICATION:
            onDispatchGroupMessage(pubMessage.content());
            break;
        case PubSubMessage::UNKNOWN:
        default:
            LOGE << "unknow pubsub redis message type. (" << pubMessage.type() << ")";
            break;
    }
}

void DispatchChannel::onDispatchGroupMessage(const std::string& message)
{
    if (!m_bAvailable) {
        return;
    }

    auto self = shared_from_this();
    auto prepareRequest = [self, message](WebsocketRequestMessage& request, std::vector<boost::any>& passing) {
        boost::ignore_unused(passing);
        int64_t startTime = nowInMicro();
        request.set_verb("PUT");
        request.set_path("/api/v1/group_message");
        request.set_body(message);

        passing.emplace_back(startTime);
        return true;
    };

    auto handleResponse = [self](const WebsocketResponseMessage& response,
                                 const std::vector<boost::any>& passing) {
        int64_t startTime = boost::any_cast<int64_t>(passing[0]);
        if (http::to_status_class(response.status()) == http::status_class::successful) {
            MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsWebsocketServiceName, "onDispatchGroupMessage",
                                                                 (nowInMicro() - startTime), 0);
            LOGI << "success to dispatch online group message for " << self->m_address;
        } else {
            MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsWebsocketServiceName, "onDispatchGroupMessage",
                                                                 (nowInMicro() - startTime), 1);
            LOGE << "failed to dispatch online group message for " << self->m_address;
        }
        return true;
    };

    dispatch(prepareRequest, handleResponse);
}

bool DispatchChannel::encrypt(const std::string& signalingKey,
                              const std::string& plaintext,
                              std::string& ciphertext)
{
    static constexpr int kCipherKeySize = 32;
    static constexpr char kCipherVersion = 0x01;
    static constexpr int kMacSize = 10;
    static constexpr int kMacKeySize = 20;

    std::string decodedSignalingKey = Base64::decode(signalingKey);

    // if signaling key is invalid, give up with plaintext
    if (decodedSignalingKey.size() < (kCipherKeySize + kMacKeySize)) {
        return false;
    }

    std::string cipherKey = decodedSignalingKey.substr(0, kCipherKeySize);
    std::string macKey = decodedSignalingKey.substr(kCipherKeySize, kMacKeySize);

    std::string iv = "";
    std::string cipherField = EvpCipher::encrypt(EvpCipher::Algo::AES_256_CBC, cipherKey, iv, plaintext);
    cipherField = std::string(&kCipherVersion, 1) + iv +  cipherField;

    std::string hmacField = Hmac::digest(Hmac::Algo::SHA256, macKey, cipherField);
    if (hmacField.length() < kMacSize) {
        return false;
    }

    ciphertext = cipherField + hmacField.substr(0, kMacSize);

    LOGD << "signaling msg debug:" << m_address
        << ":signalingkey:" << signalingKey
        << ":ciperKey:" << Base64::encode(cipherKey)
        << ":macKey:" << Base64::encode(macKey)
        << ":iv:" << Base64::encode(iv)
        << ":ciperField:" << Base64::encode(cipherField)
        << ":macField:" << Base64::encode(hmacField)
        << ":ciphertext:" << Base64::encode(ciphertext)
        << ":ciphertestSize:" << ciphertext.size();
    return true;
}

void DispatchChannel::dispatch(std::function<bool (WebsocketRequestMessage& request,
                                                   std::vector<boost::any>& passing)> before,
                               std::function<bool (const WebsocketResponseMessage& response,
                                                   const std::vector<boost::any>& passing)> after)
{
    auto self = shared_from_this();
    FiberPool::post(m_ioc, [self, before, after]() {
        std::vector<boost::any> passing;
        WebsocketRequestMessage requestMessage;
        if (!before(requestMessage, passing)) {
            return;
        }
        auto responsePromise = std::make_shared<fibers::promise<WebsocketResponseMessage>>();
        self->m_wsClient->sendRequest(requestMessage, responsePromise);
        WebsocketResponseMessage response = responsePromise->get_future().get();
        after(response, passing);
    });
}

bool DispatchChannel::publishMessage(const DispatchAddress& address, const std::string& message)
{
    boost::fibers::promise<int32_t> promise;
    boost::fibers::future<int32_t> future = promise.get_future();
    OnlineRedisManager::Instance()->publish(address.getUid(), address.getSerialized(), message,
                                            [&promise, &address, &message](int status, const redis::Reply& reply) {
                                                if (REDIS_OK != status || !reply.isInteger()) {
                                                    LOGE << "dispatcher manager publish fail, uid: " << address.getUid()
                                                         << ", message: " << message;
                                                    promise.set_value(0);
                                                    return;
                                                }
                                                promise.set_value(reply.getInteger());
                                            });
    
    return future.get() > 0;
}

void DispatchChannel::sendP2pMessage(const Envelope& envelope, boost::optional<uint64_t> storageId, bool remain)
{
    auto self = shared_from_this();

    auto prepareRequest = [self, envelope](WebsocketRequestMessage& request, std::vector<boost::any>& passing) {
        bool refreshAccount = false;
        if (self->m_address.getDeviceid() != Device::MASTER_ID) {
            refreshAccount = true;
        }
        const auto& account = boost::any_cast<Account>(self->m_wsClient->getAuthenticated(refreshAccount));
        auto authDevice = AccountsManager::getAuthDevice(account);
        if (!authDevice) {
            return false;
        }
        auto& device = *authDevice;
        auto startTime = nowInMicro();

        std::string payload;
        auto ret = self->encrypt(device.signalingkey(), envelope.SerializeAsString(), payload);
        LOGD << "signalingKey:" << self->m_address << ":" << device.signalingkey();
        if (!ret) {
            LOGE << "encrypt failed for " << self->m_address
                 << ", signaling key is " << device.signalingkey();
            return false;
        }

        request.set_verb("PUT");
        request.set_path("/api/v1/message");
        request.set_body(std::move(payload));

        passing.emplace_back(startTime);
        return true;
    };

    auto handleResponse = [self, envelope, storageId, remain](const WebsocketResponseMessage& response,
                                                              const std::vector<boost::any>& passing) {
        bool refreshAccount = false;
        if (self->m_address.getDeviceid() != Device::MASTER_ID) {
            refreshAccount = true;
        }
        const auto& account = boost::any_cast<Account>(self->m_wsClient->getAuthenticated(refreshAccount));
        auto authDevice = AccountsManager::getAuthDevice(account);
        if (!authDevice) {
            return false;
        }
        auto& device = *authDevice;
        auto startTime = boost::any_cast<int64_t>(passing[0]);

        auto dispatchManager = self->m_dispatchManager.lock();
        if (dispatchManager == nullptr) {
            LOGF << "dispatch manager is destroyed";
            return false;
        }

        if (http::to_status_class(response.status()) == http::status_class::successful) {
            if (storageId) {
                dispatchManager->getMessagesManager().del(account.uid(), static_cast<uint64_t>(*storageId));
            }

            MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsWebsocketServiceName, "sendP2pMessage",
                                                                 (nowInMicro() - startTime), 0);

            LOGD << "success to dispatch online message,"
                 << " from: " << envelope.source() << "." << envelope.sourcedevice()
                 << " source extra: " << envelope.sourceextra()
                 << " to :" << account.uid() << "." << device.id()
                 << ", duration(micro): " << nowInMicro() - startTime
                 << ", status: " << response.status();

            if (remain) {
                self->sendStoredMessages();
            }
            return true;
        }

        // just ignore noise message
        if (envelope.type() == Envelope::NOISE) {
            return true;
        }

        //TODO: requeue?

        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsWebsocketServiceName, "sendP2pMessage",
                                                             (nowInMicro() - startTime), 1);
        LOGE << "failed to dispatch online p2p message, status: " << response.status() << "," << self->m_address;

        if (storageId) {
            return false;
        }

        if (static_cast<http::status>(response.status()) == http::status::connection_closed_without_response) {
            PubSubMessage pubMessage;
            pubMessage.set_type(PubSubMessage::DELIVER);
            pubMessage.set_content(envelope.SerializeAsString());
            if (self->publishMessage(self->m_address, pubMessage.SerializeAsString())) {
                LOGD << "republished message to a new channel: " << self->m_address;
                return true;
            }
        }

        uint32_t storedMessagesCount = 0;
        auto ret = dispatchManager->getMessagesManager().store(account.uid(), device.id(), device.registrationid(),
                                                               envelope, storedMessagesCount);
        if (!ret) {
            return false;
        }

        // slave device need not offline push
        if (device.id() != Device::MASTER_ID) {
            return false;
        }

        if (envelope.type() == Envelope::RECEIPT
            || static_cast<push::Classes>(envelope.push()) == push::Classes::SILENT
            || !AccountsManager::isDevicePushable(device)) {
            return false;
        }

        // TODO: check silent
        std::string sourceInPushService = SenderUtils::getSourceInPushService(device,
                                                                              envelope,
                                                                              self->m_encryptSenderConfig);
        push::Notification notification;
        notification.chat(sourceInPushService);
        notification.setBadge(storedMessagesCount);
        notification.setTargetAddress(self->m_address);
        notification.setDeviceInfo(device);
        notification.setClass(static_cast<push::Classes>(envelope.push()));

        std::string pushType = notification.getPushType();
        if (!pushType.empty()) {
            dispatchManager->getOfflineDispatcher().dispatch(pushType, notification);
        }
        return false;
    };

    dispatch(prepareRequest, handleResponse);
}

void DispatchChannel::sendStoredMessages()
{
    if (!isSupportDispatchBatch()) {
        sendStoredMessagesLegacy();
        return;
    }

    auto self = shared_from_this();

    auto prepareRequest = [self](WebsocketRequestMessage& request, std::vector<boost::any>& passing) {
        const auto& account = boost::any_cast<Account>(self->m_wsClient->getAuthenticated());
        auto authDevice = AccountsManager::getAuthDevice(account);
        if (!authDevice) {
            return false;
        }
        auto& device = *authDevice;
        bool hasMore = false;
        int64_t startTime = nowInMicro();

        auto dispatchManager = self->m_dispatchManager.lock();
        if (dispatchManager == nullptr) {
            LOGW << "dispatch manager is destroyed";
            return false;
        }

        LOGD << "prepare stored messages for " << self->m_address;

        std::vector<StoredMessage> messages;
        auto ret = dispatchManager->getMessagesManager().get(account.uid(),
                                                             account.authdeviceid(),
                                                             kMaxCountDispatchOnce,
                                                             messages,
                                                             hasMore);
        if (!ret) {
            return false;
        }

        if (messages.empty()) {
            LOGT << "not stored message to push for " << self->m_address;
            self->sendEmpty();
            return false;
        }

        Mailbox mailbox;
        std::vector<uint64_t> toDeleteMessagesIds;
        std::vector<uint64_t> toSendMessagesIds;
        for (auto& message : messages) {
            auto&& envelope = MessagesManager::convert(message);

            if (self->isMessageStaleAndClientObsolete(message)) {
                if (self->sendReceipt(envelope, "STALE")) {
                    toDeleteMessagesIds.push_back(message.id());
                }
                continue;
            }
            toSendMessagesIds.push_back(message.id());
            *mailbox.add_envelopes() = envelope;
        }

        if (!toDeleteMessagesIds.empty()) {
            dispatchManager->getMessagesManager().del(account.uid(), toDeleteMessagesIds);
        }

        if (mailbox.envelopes().empty()) {
            LOGI << "not fresh stored message to push for " << self->m_address;
            self->sendEmpty();
            return false;
        }

        std::string payload;
        ret = self->encrypt(device.signalingkey(), mailbox.SerializeAsString(), payload);
        if (!ret) {
            LOGE << "encrypt failed for " << self->m_address
                 << ", signaling key is " << device.signalingkey();
            return false;
        }

        request.set_verb("PUT");
        request.set_path("/api/v1/messages");
        request.set_body(std::move(payload));

        LOGD << "prepare " << mailbox.envelopes().size() << "stored messages for " << self->m_address;

        passing.clear();
        passing.emplace_back(hasMore);
        passing.emplace_back(std::move(mailbox));
        passing.emplace_back(std::move(toSendMessagesIds));
        passing.emplace_back(startTime);
        return true;
    };


    auto handleResponse = [self](const WebsocketResponseMessage& response, const std::vector<boost::any>& passing) {
        const auto& account = boost::any_cast<Account>(self->m_wsClient->getAuthenticated());
        auto authDevice = AccountsManager::getAuthDevice(account);
        if (!authDevice) {
            return false;
        }
        auto& device = *authDevice;
        auto hasMore = boost::any_cast<bool>(passing[0]);
        auto mailbox = boost::any_cast<Mailbox>(passing[1]);
        auto sentMessagesIds = boost::any_cast<std::vector<uint64_t>>(passing[2]);
        auto startTime = boost::any_cast<int64_t>(passing[3]);

        if (http::to_status_class(response.status()) == http::status_class::successful) {
            auto dispatchManager = self->m_dispatchManager.lock();
            if (dispatchManager == nullptr) {
                LOGW << "dispatch manager is destroyed";
                return false;
            }

            dispatchManager->getMessagesManager().del(account.uid(), sentMessagesIds);

            MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsWebsocketServiceName, "sendStoredMessages",
                                                                 (nowInMicro() - startTime), 0);

            LOGD << "success to dispatch offline messages,"
                 << " to :" << account.uid() << "." << device.id()
                 << ", duration(micro): " << nowInMicro() - startTime;

            if (hasMore) {
                self->sendStoredMessages();
            } else {
                self->sendEmpty();
            }
        } else {
            MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsWebsocketServiceName, "sendStoredMessages",
                                                                 (nowInMicro() - startTime), 1);

            LOGD << "failed to dispatch offline messages,"
                 << " to :" << account.uid() << "." << device.id()
                 << ", duration(micro): " << nowInMicro() - startTime
                 << ", status: " << response.status();
        }
        return true;
    };

    dispatch(prepareRequest, handleResponse);
}

void DispatchChannel::sendStoredMessagesLegacy()
{
    const auto& account = boost::any_cast<Account>(m_wsClient->getAuthenticated());
    auto authDevice = AccountsManager::getAuthDevice(account);
    if (!authDevice) {
        return ;
    }

    bool hasMore = false;

    auto dispatchManager = m_dispatchManager.lock();
    if (dispatchManager == nullptr) {
        LOGW << "dispatch manager is destroyed";
        return;
    }

    LOGD << "legacy read stored messages for " << m_address;

    std::vector<StoredMessage> messages;
    auto ret = dispatchManager->getMessagesManager().get(account.uid(), account.authdeviceid(),
                                                         kMaxCountDispatchOnce, messages, hasMore);
    if (!ret || messages.empty()) {
        return;
    }

    std::vector<uint64_t> toDeleteMessagesIds;
    size_t count = messages.size();
    for (const auto& message: messages) {
        auto envelope = MessagesManager::convert(message);
        if (isMessageStaleAndClientObsolete(message)) {
            if (sendReceipt(envelope, "STALE")) {
                toDeleteMessagesIds.push_back(message.id());
            }
            continue;
        }
        sendP2pMessage(envelope, message.id(), (!(--count)) && hasMore);
    }

    if (!toDeleteMessagesIds.empty()) {
        dispatchManager->getMessagesManager().del(account.uid(), toDeleteMessagesIds);
    }

    if (!hasMore) {
        sendEmpty();
    }

    LOGD << "finish to post offline messages " << messages.size() << " for " << m_address;
}

bool DispatchChannel::sendReceipt(const Envelope& envelope, const std::string& payload)
{
    if (envelope.type() == Envelope::RECEIPT) {
        return true;
    }

    bool refreshAccount = false;
    if (m_address.getDeviceid() != Device::MASTER_ID) {
        refreshAccount = true;
    }

    const auto& account = boost::any_cast<Account>(m_wsClient->getAuthenticated(refreshAccount));
    auto authDevice = AccountsManager::getAuthDevice(account);
    if (!authDevice) {
        return false;
    }
    auto& device = *authDevice;


    Envelope receipt;
    receipt.set_type(Envelope::RECEIPT);
    receipt.set_source(account.uid());
    receipt.set_sourcedevice(device.id());
    receipt.set_sourceregistration(device.registrationid());
    receipt.set_timestamp(envelope.timestamp());
    if (!envelope.relay().empty()) {
        receipt.set_relay(envelope.relay());
    }
    receipt.set_content(payload);

    PubSubMessage pubMessage;
    pubMessage.set_type(PubSubMessage::DELIVER);
    pubMessage.set_content(receipt.SerializeAsString());

    auto dispatchManager = m_dispatchManager.lock();
    if (dispatchManager == nullptr) {
        LOGW << "dispatch manager is destroyed";
        return false;
    }

    DispatchAddress destinationAddress(envelope.source(), envelope.sourcedevice());
    if (publishMessage(destinationAddress, pubMessage.SerializeAsString())) {
        return true;
    }

    uint32_t storedMessageCount = 0;
    auto ret = dispatchManager->getMessagesManager().store(envelope.source(), envelope.sourcedevice(),
                                                           envelope.sourceregistration(), receipt, storedMessageCount);
    if (!ret) {
        LOGE << "save receipt failed, from: " << m_address
             << " to: " << envelope.source() << "." << envelope.sourcedevice();
        return false;
    }
    return true;
}

void DispatchChannel::sendEmpty()
{
    auto self = shared_from_this();

    auto prepareRequest = [self](WebsocketRequestMessage& request, std::vector<boost::any>& passing) {
        boost::ignore_unused(passing);
        request.set_verb("PUT");
        request.set_path("/api/v1/queue/empty");
        return true;
    };

    auto handleResponse = [self](const WebsocketResponseMessage& response,
                                 const std::vector<boost::any>& passing) {
        boost::ignore_unused(response, passing);
        return true;
    };

    dispatch(prepareRequest, handleResponse);
}

void DispatchChannel::sendMultiDeviceMessage(const MultiDeviceMessage& message)
{
    auto self = shared_from_this();

    auto prepareRequest = [self, message](WebsocketRequestMessage& request,
                                          std::vector<boost::any>& passing) {
        auto startTime = nowInMicro();
        std::string payload;
        if (!message.SerializeToString(&payload)) {
            LOGE << "serialize friend message failed";
            return false;
        }
        request.set_verb("PUT");
        request.set_path("/api/v1/devices");
        request.set_body(std::move(payload));

        passing.emplace_back(startTime);
        return true;
    };

    auto handleResponse = [self, message](
            const WebsocketResponseMessage& response,
            const std::vector<boost::any>& passing) {
        auto startTime = boost::any_cast<int64_t>(passing[0]);

        auto dispatchManager = self->m_dispatchManager.lock();
        if (dispatchManager == nullptr) {
            LOGF << "dispatch manager is destroyed";
            return false;
        }

        if (http::to_status_class(response.status())
                    == http::status_class::successful) {
            MetricsClient::Instance()->markMicrosecondAndRetCode(
                    kMetricsWebsocketServiceName, "sendMultiDeviceMessage",
                    (nowInMicro() - startTime), 0);

            LOGD << "success to dispatch multi device event message,"
                 << " to :" << self->m_address
                 << ", duration(micro): " << nowInMicro() - startTime
                 << ", status: " << response.status();

            return true;
        }

        MetricsClient::Instance()->markMicrosecondAndRetCode(
                kMetricsWebsocketServiceName, "sendMultiDeviceMessage",
                (nowInMicro() - startTime), 1);

        LOGE << "failed to send multi device event message to " 
             << self->m_address << ", status: " << response.status();

        switch(message.type()) {
            case MultiDeviceMessage::DeviceLogin:
            case MultiDeviceMessage::DeviceLogout:
            case MultiDeviceMessage::DeviceAvatarSync:
                break;
            case MultiDeviceMessage::DeviceAuth:
            case MultiDeviceMessage::DeviceKickedByOther:
            case MultiDeviceMessage::DeviceKickedByMaster:
            case MultiDeviceMessage::MasterLogout:
                {
                    self->m_bAvailable = false;
                    self->m_wsClient->disconnect();
                    break;
                }
            default:
                break;
        }

        return false;
    };

    dispatch(prepareRequest, handleResponse);
}

void DispatchChannel::sendFriendEventMessage(const FriendMessage& message)
{
    auto self = shared_from_this();

    auto prepareRequest = [self, message](WebsocketRequestMessage& request, 
                                          std::vector<boost::any>& passing) {
        auto startTime = nowInMicro();
        std::string payload;
        if (!message.SerializeToString(&payload)) {
            LOGE << "serialize friend message failed";
            return false;            
        }
        request.set_verb("PUT");
        request.set_path("/api/v1/friends");
        request.set_body(std::move(payload));

        passing.emplace_back(startTime);
        return true;  
    };

    auto handleResponse = [self, message](
            const WebsocketResponseMessage& response,
            const std::vector<boost::any>& passing) {

        bool refreshAccount = false;
        if (self->m_address.getDeviceid() != Device::MASTER_ID) {
            refreshAccount = true;
        }

        const auto& account = boost::any_cast<Account>(self->m_wsClient->getAuthenticated(refreshAccount));
        auto authDevice = AccountsManager::getAuthDevice(account);
        if (!authDevice) {
            return false;
        }
        auto& device = *authDevice;
        auto startTime = boost::any_cast<int64_t>(passing[0]);

        auto dispatchManager = self->m_dispatchManager.lock();
        if (dispatchManager == nullptr) {
            LOGF << "dispatch manager is destroyed";
            return false;
        }

        if (http::to_status_class(response.status()) 
                    == http::status_class::successful) {
            MetricsClient::Instance()->markMicrosecondAndRetCode(
                    kMetricsWebsocketServiceName, "sendFriendEventMessage",
                    (nowInMicro() - startTime), 0);
            
            LOGD << "success to dispatch friend event message,"
                 << " to :" << account.uid() << "." << device.id()
                 << ", duration(micro): " << nowInMicro() - startTime
                 << ", status: " << response.status();

            return true;
        }

        MetricsClient::Instance()->markMicrosecondAndRetCode(
                kMetricsWebsocketServiceName, "sendFriendEventMessage", 
                (nowInMicro() - startTime), 1);

        LOGE << "failed to dispatch friend event message to " 
             << self->m_address << ", status: " << response.status();

        if (static_cast<http::status>(response.status()) 
                    == http::status::connection_closed_without_response) {
            PubSubMessage msg;
            msg.set_type(PubSubMessage::FRIEND);
            msg.set_content(message.SerializeAsString());
            std::string msgStr = msg.SerializeAsString();
            if (self->publishMessage(self->m_address, msgStr)) {
                LOGD << "friend event message is re-published to " 
                     << self->m_address;
                return true;
            }
        }
        
        dao::Contacts& contacts(dispatchManager->getContacts());
        for (int i = 0; i < message.requests_size(); ++i) {
            const FriendRequest& entry = message.requests(i);
            int64_t eventId = 0;
            dao::ErrorCode ec = contacts.addFriendEvent(
                    self->m_address.getUid(), 
                    dao::FriendEventType::FRIEND_REQUEST, 
                    entry.SerializeAsString(), 
                    eventId);
            if (ec != dao::ERRORCODE_SUCCESS) {
                LOGE << "failed to store friend request for " 
                     << self->m_address.getUid() << ", ec: " << ec;
            }
        }

        for (int i = 0; i < message.replies_size(); ++i) {
            const FriendReply& entry = message.replies(i);
            int64_t eventId = 0;
            dao::ErrorCode ec = contacts.addFriendEvent(
                    self->m_address.getUid(), 
                    dao::FriendEventType::FRIEND_REPLY, 
                    entry.SerializeAsString(), 
                    eventId);
            if (ec != dao::ERRORCODE_SUCCESS) {
                LOGE << "failed to store friend reply for " 
                     << self->m_address.getUid() << ", ec: " << ec;
            }
        }

        for (int i = 0; i < message.deletes_size(); ++i) {
            const DeleteFriend& entry = message.deletes(i);
            int64_t eventId = 0;
            dao::ErrorCode ec = contacts.addFriendEvent(
                    self->m_address.getUid(), 
                    dao::FriendEventType::DELETE_FRIEND, 
                    entry.SerializeAsString(), 
                    eventId);
            if (ec != dao::ERRORCODE_SUCCESS) {
                LOGE << "failed to store friend deletion for " 
                     << self->m_address.getUid() << ", ec: " << ec;
            }
        }

        return false;
    };

    dispatch(prepareRequest, handleResponse);
}

void DispatchChannel::sendStoredFriendEventMessages(const std::set<dao::FriendEventType>& types)
{
    static const int kMaxGetFriendEventsCount = 100;
    auto self = shared_from_this();

    auto prepareRequest = [self, types](WebsocketRequestMessage& request, 
                                 std::vector<boost::any>& passing) {
        auto startTime = nowInMicro();
        passing.emplace_back(startTime);
        passing.emplace_back(std::vector<int64_t>());
        passing.emplace_back(std::vector<int64_t>());
        passing.emplace_back(std::vector<int64_t>());
        passing.emplace_back(std::set<dao::FriendEventType>());

        auto& requestIds = boost::any_cast<std::vector<int64_t>&>(passing[1]);
        auto& replyIds = boost::any_cast<std::vector<int64_t>&>(passing[2]);
        auto& deletionIds = boost::any_cast<std::vector<int64_t>&>(passing[3]);
        auto& moreEventTypes = boost::any_cast<std::set<dao::FriendEventType>&>(passing[4]);

        auto dispatchManager = self->m_dispatchManager.lock();
        if (dispatchManager == nullptr) {
            LOGF << "dispatch manager is destroyed";
            return false;
        }

        LOGD << "prepare stored friend event messages for " << self->m_address;

        dao::Contacts& contacts(dispatchManager->getContacts());
        FriendMessage friendMsg;

        std::vector<dao::FriendEvent> events;
        events.reserve(kMaxGetFriendEventsCount);

        if (types.count(dao::FriendEventType::FRIEND_REQUEST) != 0) {
            events.clear();
            dao::ErrorCode ec = contacts.getFriendEvents(
                    self->m_address.getUid(), 
                    dao::FriendEventType::FRIEND_REQUEST, 
                    kMaxGetFriendEventsCount, 
                    events);
            if (ec != dao::ERRORCODE_SUCCESS && ec != dao::ERRORCODE_NO_SUCH_DATA) {
                LOGE << "failed to get friend request for account " 
                     << self->m_address.getUid() << ", ec: " << ec;
                return false;
            }
            if (!events.empty()) {
                for (const dao::FriendEvent& entry : events) {
                    FriendRequest* r = friendMsg.add_requests();
                    if (!r->ParseFromString(entry.data)) {
                        LOGE << "failed to parse friend request, id: " << entry.id 
                            << ", uid: " << self->m_address.getUid();
                        friendMsg.mutable_requests()->RemoveLast();
                        continue;
                    }
                    requestIds.push_back(entry.id);
                }
                if (events.size() >= kMaxGetFriendEventsCount) {
                    moreEventTypes.insert(dao::FriendEventType::FRIEND_REQUEST);
                }
            }
        }

        if (types.count(dao::FriendEventType::FRIEND_REPLY) != 0) {
            events.clear();
            dao::ErrorCode ec = contacts.getFriendEvents(
                    self->m_address.getUid(), 
                    dao::FriendEventType::FRIEND_REPLY, 
                    kMaxGetFriendEventsCount, 
                    events);
            if (ec != dao::ERRORCODE_SUCCESS && ec != dao::ERRORCODE_NO_SUCH_DATA) {
                LOGE << "failed to get friend reply for account " 
                     << self->m_address.getUid() << ", ec: " << ec;
                return false;
            }
            if (!events.empty()) {
                for (const dao::FriendEvent& entry : events) {
                    FriendReply* r = friendMsg.add_replies();
                    if (!r->ParseFromString(entry.data)) {
                        LOGE << "failed to parse friend reply, id: " << entry.id 
                            << ", uid: " << self->m_address.getUid();
                        friendMsg.mutable_replies()->RemoveLast();
                        continue;
                    }
                    replyIds.push_back(entry.id);
                }
                if (events.size() >= kMaxGetFriendEventsCount) {
                    moreEventTypes.insert(dao::FriendEventType::FRIEND_REPLY);
                }
            }
        }

        if (types.count(dao::FriendEventType::DELETE_FRIEND) != 0) {
            events.clear();
            dao::ErrorCode ec = contacts.getFriendEvents(
                    self->m_address.getUid(), 
                    dao::FriendEventType::DELETE_FRIEND, 
                    kMaxGetFriendEventsCount, 
                    events);
            if (ec != dao::ERRORCODE_SUCCESS && ec != dao::ERRORCODE_NO_SUCH_DATA) {
                LOGE << "failed to get friend deletion for account " 
                     << self->m_address.getUid() << ", ec: " << ec;
                return false;
            }
            if (!events.empty()) {
                for (const dao::FriendEvent& entry : events) {
                    DeleteFriend* d = friendMsg.add_deletes();
                    if (!d->ParseFromString(entry.data)) {
                        LOGE << "failed to parse friend deletion, id: " << entry.id 
                            << ", uid: " << self->m_address.getUid();
                        friendMsg.mutable_deletes()->RemoveLast();
                        continue;
                    }
                    deletionIds.push_back(entry.id);
                }
                if (events.size() >= kMaxGetFriendEventsCount) {
                    moreEventTypes.insert(dao::FriendEventType::DELETE_FRIEND);
                }
            }
        }

        if (friendMsg.requests().empty() && 
            friendMsg.replies().empty() && 
            friendMsg.deletes().empty()) {
            return false;
        }

        std::string payload;
        if (!friendMsg.SerializeToString(&payload)) {
            LOGE << "serialize friend message failed";
            return false;            
        }
        request.set_verb("PUT");
        request.set_path("/api/v1/friends");
        request.set_body(std::move(payload));

        return true;  
    };

    auto handleResponse = [self](const WebsocketResponseMessage& response,
                                 const std::vector<boost::any>& passing) {

        bool refreshAccount = false;
        if (self->m_address.getDeviceid() != Device::MASTER_ID) {
            refreshAccount = true;
        }

        const auto& account = boost::any_cast<Account>(self->m_wsClient->getAuthenticated(refreshAccount));
        auto authDevice = AccountsManager::getAuthDevice(account);
        if (!authDevice) {
            return false;
        }
        auto& device = *authDevice;
        
        auto startTime = boost::any_cast<int64_t>(passing[0]);
        const auto& requestIds = boost::any_cast<const std::vector<int64_t>&>(passing[1]);
        const auto& replyIds = boost::any_cast<const std::vector<int64_t>&>(passing[2]);
        const auto& deletionIds = boost::any_cast<const std::vector<int64_t>&>(passing[3]);
        const auto& moreEventTypes = boost::any_cast<const std::set<dao::FriendEventType>&>(passing[4]);

        auto dispatchManager = self->m_dispatchManager.lock();
        if (dispatchManager == nullptr) {
            LOGF << "dispatch manager is destroyed";
            return false;
        }

        dao::Contacts& contacts(dispatchManager->getContacts());
        
        if (http::to_status_class(response.status()) 
                    == http::status_class::successful) {
            MetricsClient::Instance()->markMicrosecondAndRetCode(
                    kMetricsWebsocketServiceName, "sendStoredFriendEventMessages",
                    (nowInMicro() - startTime), 0);
            
            LOGD << "success to dispatch " 
                 << requestIds.size() << " stored friend requests, "
                 << replyIds.size() << " stored friend replies, "
                 << deletionIds.size() << " stored friend deletions"
                 << " to :" << account.uid() << "." << device.id()
                 << ", duration(micro): " << nowInMicro() - startTime
                 << ", status: " << response.status();

            dao::ErrorCode ec;
            bool hasError = false;

            if (!requestIds.empty()) {
                ec = contacts.delFriendEvents(self->m_address.getUid(), 
                        dao::FriendEventType::FRIEND_REQUEST, requestIds);
                if (ec != dao::ERRORCODE_SUCCESS && ec != dao::ERRORCODE_NO_SUCH_DATA) {
                    LOGE << "failed to delete friend requests from DB, uid: " 
                         << self->m_address.getUid() << ", ec: " << ec;
                    hasError = true;
                }
            }

            if (!replyIds.empty()) {
                ec = contacts.delFriendEvents(self->m_address.getUid(),
                        dao::FriendEventType::FRIEND_REPLY, replyIds);
                if (ec != dao::ERRORCODE_SUCCESS && ec != dao::ERRORCODE_NO_SUCH_DATA) {
                    LOGE << "failed to delete friend replies from DB, uid: " 
                         << self->m_address.getUid() << ", ec: " << ec;
                    hasError = true;
                }
            }

            if (!deletionIds.empty()) {
                ec = contacts.delFriendEvents(self->m_address.getUid(),
                        dao::FriendEventType::DELETE_FRIEND, deletionIds);
                if (ec != dao::ERRORCODE_SUCCESS && ec != dao::ERRORCODE_NO_SUCH_DATA) {
                    LOGE << "failed to delete friend deletions from DB, uid: " 
                         << self->m_address.getUid() << ", ec: " << ec;
                    hasError = true;
                }
            }

            if (!hasError && !moreEventTypes.empty()) {
                self->sendStoredFriendEventMessages(moreEventTypes);
            }

            return true;
        }

        MetricsClient::Instance()->markMicrosecondAndRetCode(
                kMetricsWebsocketServiceName, "sendStoredFriendEventMessages", 
                (nowInMicro() - startTime), 1);

        LOGE << "failed to dispatch " 
             << requestIds.size() << " stored friend requests, "
             << replyIds.size() << " stored friend replies, "
             << deletionIds.size() << " stored friend deletions to " 
             << self->m_address << ", status: " << response.status();
        
        return false;
    };

    dispatch(prepareRequest, handleResponse);
}

bool DispatchChannel::isSupportDispatchBatch()
{
    bool refreshAccount = false;
    if (m_address.getDeviceid() != Device::MASTER_ID) {
        refreshAccount = true;
    }

    const auto& account = boost::any_cast<Account>(m_wsClient->getAuthenticated(refreshAccount));
    auto authDevice = AccountsManager::getAuthDevice(account);
    if (!authDevice) {
        return false;
    }
    auto& device = *authDevice;
    auto& cliVer = device.clientversion();
    
    if (cliVer.ostype() == ClientVersion::OSTYPE_IOS) {
        return (cliVer.bcmbuildcode() >= 1235);
    } else if (cliVer.ostype() == ClientVersion::OSTYPE_ANDROID) {
        return (cliVer.bcmbuildcode() >= 1105);
    } else {
        return false;
    }
}

bool DispatchChannel::isMessageStaleAndClientObsolete(const StoredMessage& message)
{
    bool refreshAccount = false;
    if (m_address.getDeviceid() != Device::MASTER_ID) {
        refreshAccount = true;
    }

    const auto& account = boost::any_cast<Account>(m_wsClient->getAuthenticated(refreshAccount));
    auto authDevice = AccountsManager::getAuthDevice(account);
    if (!authDevice) {
        return false;
    }
    auto& device = *authDevice;

    return message.destinationregistrationid() != 0
            && message.destinationregistrationid() != device.registrationid()
            && message.source() != ""
            && !SenderUtils::isClientVersionSupportEncryptSender(device, m_encryptSenderConfig);
}

std::shared_ptr<WebsocketSession> DispatchChannel::getSession()
{
    return m_wsClient;
}

}
