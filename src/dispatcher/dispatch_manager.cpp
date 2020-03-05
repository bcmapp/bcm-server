#include "dispatch_manager.h"
#include <utils/log.h>
#include <redis/hiredis_client.h>
#include <proto/dao/device.pb.h>
#include <dispatcher/dispatch_channel.h>
#include "redis/online_redis_manager.h"
#include "redis/reply.h"
#include "redis/redis_manager.h"
#include "../config/group_store_format.h"

namespace bcm {

DispatchManager::DispatchManager(DispatcherConfig config,
                                 std::shared_ptr<MessagesManager> messagesManager,
                                 std::shared_ptr<OfflineDispatcher> offlineDispatcher,
                                 std::shared_ptr<dao::Contacts> contacts,
                                 EncryptSenderConfig& cfg)
        : m_bridge(1)
        , m_workerPool(static_cast<size_t>(config.concurrency))
        , m_offlineDispatcher(std::move(offlineDispatcher))
        , m_messagesManager(std::move(messagesManager))
        , m_contacts(std::move(contacts))
        , m_encryptSenderConfig(cfg)
{
}

DispatchManager::~DispatchManager() = default;

void DispatchManager::start()
{
    m_bridge.run("dispatch.bridge");
    m_workerPool.run("dispatch.worker");
    m_messageDispatching = true;
    FiberPool::post(m_bridge.getIOContext(), std::bind(&DispatchManager::dispatchMessages, shared_from_this()));
}

void DispatchManager::stop()
{
    m_messageDispatching = false;
    m_bridge.stop();
}

void DispatchManager::onSubscribe(const std::string& serializedAddress)
{
    if (serializedAddress.empty()) {
        return;
    }

    auto address = DispatchAddress::deserialize(serializedAddress);
    if (!address) {
        LOGD << "not a dispatch address " << serializedAddress;
        return;
    }

    LOGI << "success to subscribe " << serializedAddress << " from redis and dispatch handled";

    enqueueMessage(Message(Message::TYPE_REDIS_SUBSCRIBED, address));
}

void DispatchManager::onUnsubscribe(const std::string& chan)
{
    LOGI << "success to unsubscribe " << chan << " from redis";
}

void DispatchManager::onMessage(const std::string& serializedAddress, const std::string& serializedMessage)
{
    if (serializedAddress.empty() || serializedMessage.empty()) {
        return;
    }

    auto address = DispatchAddress::deserialize(serializedAddress);
    if (!address) {
        LOGD << "not a dispatch address " << serializedAddress << ", message size: " << serializedMessage.size();
        return;
    }

    enqueueMessage(Message(Message::TYPE_REDIS_MESSAGE, address, serializedMessage));
    LOGI << "receive subscribe msg: " << serializedMessage << ", from channel: " 
         << serializedAddress << ", and notify dispatcher";
}

void DispatchManager::onError(int status)
{
    LOGE << "dispatcher redis subscribe error, status: " << status;
}

uint64_t DispatchManager::subscribe(const DispatchAddress& address,
                                    std::shared_ptr<WebsocketSession> wsClient)
{
    LOGI << "receive request to subscribe channel.(" << address << ")";

    auto newDispatcher =
            std::make_shared<DispatchChannel>(m_workerPool.getIOContext(),
                                              address,
                                              wsClient,
                                              shared_from_this(),
                                              m_encryptSenderConfig);

    auto oldDispatcher = replaceDispatcher(address, newDispatcher);

    // sending a connected message, another server should recive it and kick off the obsolete connection
    PubSubMessage message;
    message.set_type(PubSubMessage::CONNECTED);
    message.set_content(std::to_string(newDispatcher->getIdentity()));
    std::string connectNotify = message.SerializeAsString();
    
    publish(address, connectNotify);
    
    if (!RedisDbManager::Instance()->del(address.getUid(), bcm::REDISDB_KEY_APNS_UID_BADGE_PREFIX + address.getUid())) {
        LOGE << "delete RedisDbManager push counter false, uid: " << address.getUid();
    }
    
    if (!RedisClientSync::Instance()->publishRes(
            address.getSerializedForOnlineNotify(), connectNotify)) {
        LOGW << "failed to publish connect notify to address: " 
             << address.getSerializedForOnlineNotify();
    }

    if (OnlineRedisManager::Instance()->subscribe(address.getUid(), address.getSerialized(), this)) {
        LOGI << "success to subscribe dispatch channel.(" << address << " " << newDispatcher << ")";
    } else {
        LOGE << "failed to subscribe dispatch channel.(" << address << " " << newDispatcher << ")";
        // will resubscribe if connected.
    }

    onUserStatusChange(address, true);

    if (oldDispatcher) {
        LOGI << "unsubscribe old dispatch channel.(" << address << " " << oldDispatcher << ")";
        oldDispatcher->onDispatchUnsubscribed(true);
    }

    return newDispatcher->getIdentity();
}

void DispatchManager::unsubscribe(const DispatchAddress& address, uint64_t dispatcherId)
{
    LOGI << "receive request to unsubscribe channel.(" << address << ")";

    auto dispatcher = delDispatcher(address, dispatcherId);
    if (dispatcher == nullptr) {
        return;
    }

    OnlineRedisManager::Instance()->unsubscribe(address.getUid(), address.getSerialized());
    dispatcher->onDispatchUnsubscribed(false);
    onUserStatusChange(address, false);

    LOGI << "unsubscribe dispatch channel.(" << address << " " << dispatcher << ")";
}


bool DispatchManager::publish(const DispatchAddress& address, const std::string& message)
{
    std::promise<int32_t> promise;
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

    return promise.get_future().get() > 0;
}

void DispatchManager::kick(const DispatchAddress& address)
{
    OnlineRedisManager::Instance()->unsubscribe(address.getUid(), address.getSerialized());

    auto dispatcher = delDispatcher(address);
    if (dispatcher == nullptr) {
        LOGW << "not unsubscribe since not find.(" << address << ")";
        return;
    }

    dispatcher->onDispatchUnsubscribed(true);
    onUserStatusChange(address, false);

    LOGI << "unsubscribe dispatch channel.(" << address << " " << dispatcher << ")";
}

void DispatchManager::sendGroupMessage(const std::vector<DispatchAddress>& destinations, const std::string& message)
{
    if (destinations.empty() || message.empty()) {
        LOGW << "miss to send group message,since message or destination empty.";
        return;
    }

    std::vector<Message> tmpQueue;
    for (const auto& destination : destinations) {
        tmpQueue.emplace_back(Message::TYPE_GROUP_MESSAGE, destination, message);
    }
    enqueueMessages(tmpQueue);
}

void DispatchManager::sendGroupMessage(const std::vector<GroupMessages>& messages)
{
    if (messages.empty()) {
        return;
    }

    std::vector<Message> tmpQueue;
    for (const auto& msg : messages) {
        if (msg.message == nullptr || msg.destinations == nullptr) {
            continue;
        }
        for (const auto& destination : *(msg.destinations)) {
            tmpQueue.emplace_back(Message::TYPE_GROUP_MESSAGE, destination, *(msg.message));
        }
    }
    if (tmpQueue.empty()) {
        LOGW << "miss to send group message,since message or destination empty.";
        return;
    }

    enqueueMessages(tmpQueue);
}

bool DispatchManager::hasLocalSubscription(const DispatchAddress& address)
{
    return OnlineRedisManager::Instance()->isSubscribed(address.getUid(), address.getSerialized());
}

void DispatchManager::registerUserStatusListener(IUserStatusListener* listener)
{
    m_userStatusListeners.insert(listener);
}

void DispatchManager::unregisterUserStatusListener(IUserStatusListener* listener)
{
    m_userStatusListeners.erase(listener);
}

void DispatchManager::onUserStatusChange(const DispatchAddress& address, bool bOnline)
{
    for (auto& listener : m_userStatusListeners) {
        if (bOnline) {
            listener->onUserOnline(address);
        } else {
            listener->onUserOffline(address);
        }
    }
}

void DispatchManager::enqueueMessage(Message&& message)
{
    std::unique_lock<std::mutex> lk(m_messageMutex);
    bool wakeup = m_messageQueue.empty();
    m_messageQueue.emplace_back(std::move(message));
    if (wakeup) {
        m_messageCond.notify_one();
    }
}

void DispatchManager::enqueueMessages(std::vector<Message>& messages)
{
    std::unique_lock<std::mutex> lk(m_messageMutex);
    bool wakeup = m_messageQueue.empty();
    m_messageQueue.insert(m_messageQueue.end(), messages.begin(), messages.end());
    if (wakeup) {
        m_messageCond.notify_one();
    }
}

void DispatchManager::dispatchMessages()
{
    std::vector<Message> localMessageQueue;
    while (m_messageDispatching) {
        {
            std::unique_lock<std::mutex> lk(m_messageMutex);
            m_messageCond.wait(lk, [this]() {
                return !m_messageQueue.empty();
            });

            localMessageQueue.clear();
            localMessageQueue.swap(m_messageQueue);
        }

        for (const auto& message : localMessageQueue) {
            switch (message.type) {
                case Message::TYPE_REDIS_CONNECTED: {
                    break;
                }
                case Message::TYPE_REDIS_DISCONNECTED: {
                    // reconnect?
                    break;
                }
                case Message::TYPE_REDIS_SUBSCRIBED: {
                    BOOST_ASSERT(message.address != boost::none);
                    auto dispatcher = getDispatcher(*message.address);
                    if (!dispatcher) {
                        LOGW << "not find target dispatcher for " << *message.address;
                        break;
                    }
                    dispatcher->onDispatchSubscribed();
                    break;
                }
                case Message::TYPE_REDIS_UNSUBSCRIBED: {
                    BOOST_ASSERT(message.address != boost::none);
                    auto dispatcher = getDispatcher(*message.address);
                    if (!dispatcher) {
                        LOGW << "not find target dispatcher for " << *message.address;
                        break;
                    }
                    dispatcher->onDispatchUnsubscribed(false);
                    break;
                }
                case Message::TYPE_REDIS_MESSAGE: {
                    BOOST_ASSERT(message.address != boost::none);
                    auto dispatcher = getDispatcher(*message.address);
                    if (!dispatcher) {
                        LOGW << "not find target dispatcher for " << *message.address;
                        break;
                    }
                    dispatcher->onDispatchRedisMessage(message.content);
                    break;
                }
                case Message::TYPE_GROUP_MESSAGE: {
                    BOOST_ASSERT(message.address != boost::none);
                    auto dispatcher = getDispatcher(*message.address);
                    if (!dispatcher) {
                        LOGW << "not find target dispatcher for " << *message.address;
                        break;
                    }
                    dispatcher->onDispatchGroupMessage(message.content);
                    break;
                }
                default: {
                    break;
                }
            }
        }
    }
}

std::shared_ptr<IDispatcher> DispatchManager::replaceDispatcher(const DispatchAddress& address,
                                                                std::shared_ptr<IDispatcher> dispatcher)
{
    std::unique_lock<fibers::mutex> l(m_dispatchersMutex);
    std::shared_ptr<IDispatcher> oldDispatcher(nullptr);
    auto it = m_dispatchers.find(address);
    if (it != m_dispatchers.end()) {
        oldDispatcher = it->second;
    }
    m_dispatchers[address] = dispatcher;
    return oldDispatcher;
}

std::shared_ptr<IDispatcher> DispatchManager::delDispatcher(const DispatchAddress& address, uint64_t dispatcherId)
{
    std::unique_lock<fibers::mutex> l(m_dispatchersMutex);
    auto it = m_dispatchers.find(address);
    if (it == m_dispatchers.end()) {
        return nullptr;
    }
    auto dispatcher = it->second;
    if (dispatcherId != UINT64_MAX && (dispatcherId != dispatcher->getIdentity())) {
        return nullptr;
    }
    m_dispatchers.erase(address);
    return dispatcher;
}

std::shared_ptr<IDispatcher> DispatchManager::getDispatcher(const DispatchAddress& address)
{
    std::unique_lock<fibers::mutex> l(m_dispatchersMutex);
    auto it = m_dispatchers.find(address);
    if (it != m_dispatchers.end()) {
        return it->second;
    }
    return nullptr;
}

int64_t DispatchManager::getDispatchCount()
{
    std::unique_lock<fibers::mutex> l(m_dispatchersMutex);
    return (int64_t) m_dispatchers.size();
}

}
