#pragma once

#include "message_entities.h"
#include <proto/message/message_protocol.pb.h>
#include <http/http_router.h>
#include <store/accounts_manager.h>
#include <dispatcher/dispatch_manager.h>
#include <push/push_service.h>
#include "config/size_check_config.h"
#include "config/multi_device_config.h"

namespace bcm {

class MessageController 
    : public HttpRouter::Controller
    , public std::enable_shared_from_this<MessageController> {
public:
    MessageController(std::shared_ptr<AccountsManager> accountsManager,
                      std::shared_ptr<OfflineDispatcher> offlineDispatcher,
                      std::shared_ptr<DispatchManager> dispatchManager,
                      const EncryptSenderConfig& cfg,
                      const MultiDeviceConfig& multiDeviceCfg,
                      const SizeCheckConfig& scCfg);

    ~MessageController();

private:
    void addRoutes(HttpRouter& router) override;

    void sendMessage(HttpContext& context);

    bool sendLocalMessage(const Account& source, const Account& destination, const Device& destinationDevice,
                          uint64_t timestamp, const IncomingMessage& message, const std::string& sourceExtra);
    Envelope createEnvelope(const Account& source, uint64_t timestamp,
                            const IncomingMessage& message, const std::string& sourceExtra);

    void getDestination(const std::string& destinationUid, Account& destination, http::status& result);
    bool validateCompleteDeviceList(const Account& destination, const IncomingMessageList& messageList,
                                    bool isSyncMessage, MismatchedDevices& mismatchedDevices);
    bool validateRegistrationIds(const Account& destination, const IncomingMessageList& messageList,
                                 StaleDevices& staleDevices);

    std::string generateSourceExtra(const std::string& source, const Account& destination, const Device& device);

private:
    std::shared_ptr<AccountsManager> m_accountsManager;
    std::shared_ptr<OfflineDispatcher> m_offlineDispatcher;
    std::shared_ptr<DispatchManager> m_dispatchManager;
    EncryptSenderConfig m_encryptSenderConfig;
    MultiDeviceConfig m_multiDeviceConfig;
    SizeCheckConfig m_sizeCheckConfig;
};

}

