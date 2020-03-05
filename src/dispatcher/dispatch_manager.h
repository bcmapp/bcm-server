#include <utility>

#pragma once

#include "idispatcher.h"
#include "dispatch_address.h"
#include "offline_dispatcher.h"
#include "config/encrypt_sender.h"
#include <redis/iasync_redis_event.h>
#include <fiber/fiber_pool.h>
#include <config/dispatcher_config.h>
#include <store/messages_manager.h>
#include <store/accounts_manager.h>

namespace bcm {

class WebsocketSession;

class DispatchManager : public std::enable_shared_from_this<DispatchManager>
                      , public redis::AsyncConn::ISubscriptionHandler {
public:
    struct GroupMessages {
        typedef std::shared_ptr<std::set<DispatchAddress>> AddressesPtr;
        typedef std::shared_ptr<std::string> MessagePtr;
        GroupMessages(std::shared_ptr<std::set<DispatchAddress>> d, std::shared_ptr<std::string> m) 
            : destinations(std::move(d)), message(std::move(m))
        {

        }
        AddressesPtr destinations;
        MessagePtr message;
    };
    DispatchManager(DispatcherConfig config,
                    std::shared_ptr<MessagesManager> messagesManager,
                    std::shared_ptr<OfflineDispatcher> offlineDispatcher,
                    std::shared_ptr<dao::Contacts> contacts,
                    EncryptSenderConfig& cfg);
    virtual ~DispatchManager();

    void start();
    void stop();

    //asyncConn handler
    void onSubscribe(const std::string& chan) override;
    void onUnsubscribe(const std::string& chan) override;
    void onMessage(const std::string& chan, const std::string& msg) override;
    void onError(int status) override;

    void sendGroupMessage(const std::vector<DispatchAddress>& destinations, const std::string& message);
    void sendGroupMessage(const std::vector<GroupMessages>& messages);

    //dispatcher service api.
    uint64_t subscribe(const DispatchAddress& address, std::shared_ptr<WebsocketSession> wsClient);
    void unsubscribe(const DispatchAddress& address, uint64_t dispatcherId);
    bool publish(const DispatchAddress& address, const std::string& message);
    void kick(const DispatchAddress& address);
    bool hasLocalSubscription(const DispatchAddress& address);

    OfflineDispatcher& getOfflineDispatcher() { return *m_offlineDispatcher; }
    MessagesManager& getMessagesManager() { return *m_messagesManager; }
    dao::Contacts& getContacts() { return *m_contacts; }

public:
    class IUserStatusListener {
    public:
        virtual void onUserOnline(const DispatchAddress& user) = 0;
        virtual void onUserOffline(const DispatchAddress& user) = 0;
    };

    void registerUserStatusListener(IUserStatusListener* listener);
    void unregisterUserStatusListener(IUserStatusListener* listener);

private:
    void onUserStatusChange(const DispatchAddress& address, bool bOnline);

private:
    struct Message {
        enum Type {
            TYPE_REDIS_CONNECTED,
            TYPE_REDIS_DISCONNECTED,
            TYPE_REDIS_SUBSCRIBED,
            TYPE_REDIS_UNSUBSCRIBED,
            TYPE_REDIS_MESSAGE,
            TYPE_GROUP_MESSAGE,
        };

        Type type;
        boost::optional<DispatchAddress> address;
        std::string content;

        Message(Type type_, boost::optional<DispatchAddress> address_)
            : type(type_), address(std::move(address_)) {}

        Message(Type type_, boost::optional<DispatchAddress> address_, std::string content_)
            : type(type_), address(std::move(address_)), content(std::move(content_)) {}

        Message(const Message&) = default;
        Message(Message&&) = default;
        Message& operator=(const Message&) = default;
    };

    void enqueueMessage(Message&& message);
    void enqueueMessages(std::vector<Message>& messages);
    void dispatchMessages();

    /* return deleted dispatcher or nullptr if not exist
     * if dispatcherId is UINT64_MAX, do not check it
     */
    std::shared_ptr<IDispatcher> delDispatcher(const DispatchAddress& address, uint64_t dispatcherId = UINT64_MAX);

public:
    std::shared_ptr<IDispatcher> getDispatcher(const DispatchAddress& address);
    int64_t getDispatchCount();
    
#ifndef NOISE_TEST
private:
#endif
    // return the old dispatcher or nullptr if not exist
    std::shared_ptr<IDispatcher> replaceDispatcher(const DispatchAddress& address,
                                                   std::shared_ptr<IDispatcher> dispatcher);

private:
    fibers::mutex m_dispatchersMutex;
    std::map<DispatchAddress, std::shared_ptr<IDispatcher>> m_dispatchers;
    FiberPool m_bridge;
    FiberPool m_workerPool;
    std::shared_ptr<OfflineDispatcher> m_offlineDispatcher;
    std::shared_ptr<MessagesManager> m_messagesManager;
    std::shared_ptr<dao::Contacts> m_contacts;

    std::set<IUserStatusListener*> m_userStatusListeners;

    std::mutex m_messageMutex;
    std::condition_variable m_messageCond;
    std::vector<Message> m_messageQueue;
    bool m_messageDispatching{false};
    EncryptSenderConfig m_encryptSenderConfig;
};

}
