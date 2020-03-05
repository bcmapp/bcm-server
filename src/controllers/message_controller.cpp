#include "message_controller.h"
#include <proto/dao/account.pb.h>
#include <proto/dao/error_code.pb.h>
#include <crypto/base64.h>
#include <utils/log.h>
#include <utils/time.h>
#include <push/push_service.h>
#include <fiber/fiber_pool.h>
#include <store/accounts_manager.h>
#include <metrics_client.h>
#include "bloom/bloom_filters.h"
#include "features/bcm_features.h"
#include <utils/sender_utils.h>
#include "crypto/hex_encoder.h"
#include "http/custom_http_status.h"

namespace bcm {

using namespace metrics;

static constexpr char kMetricsMessageServiceName[] = "message";

MessageController::MessageController(std::shared_ptr<AccountsManager> accountsManager,
                                     std::shared_ptr<OfflineDispatcher> offlineDispatcher,
                                     std::shared_ptr<DispatchManager> dispatchManager,
                                     const EncryptSenderConfig& cfg,
                                     const MultiDeviceConfig& multiDeviceCfg,
                                     const SizeCheckConfig& scCfg)
    : m_accountsManager(std::move(accountsManager))
    , m_offlineDispatcher(std::move(offlineDispatcher))
    , m_dispatchManager(std::move(dispatchManager))
    , m_encryptSenderConfig(cfg)
    , m_multiDeviceConfig(multiDeviceCfg)
    , m_sizeCheckConfig(scCfg)
{

}

MessageController::~MessageController() = default;

void MessageController::addRoutes(bcm::HttpRouter& router)
{
    router.add(http::verb::put, "/v1/messages/:uid", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&MessageController::sendMessage, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<IncomingMessageList>, new JsonSerializerImp<SendMessageResponse>);
}

void MessageController::sendMessage(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto& source = boost::any_cast<Account&>(context.authResult);
    auto& messageList = boost::any_cast<IncomingMessageList&>(context.requestEntity);
    auto& destinationUid = context.pathParams.at(":uid");
    auto& response = context.response;

    auto handleJsonError = [&](http::status status, const boost::any& body, JsonSerializer&& serializer) {
        std::string payload;
        if (serializer.serialize(body, payload)) {
            response.result(status);
            response.set(http::field::content_type, "application/json; charset=utf-8");
            response.set(http::field::content_length, payload.size());
            response.body() = payload;
        }
    };

    auto bSyncMessage = (source.uid() == destinationUid);

    if (!messageList.relay.empty()) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsMessageServiceName,
            "sendMessage", (nowInMicro() - dwStartTime), 1001);
        // TODO: support sending relay message
        response.result(http::status::bad_request);
        LOGW << "relay message is not support now: " << source.uid() << " -> " << destinationUid << "";
        return;
    }

    for (const auto& msg : messageList.messages) {
        if (msg.content.size() > m_sizeCheckConfig.messageSize) {
            MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsMessageServiceName,
                                                                 "sendMessage",
                                                                 (nowInMicro() - dwStartTime),
                                                                 1006);
            LOGE << "message size " << msg.content.size()
                 << " is more than the limit " << m_sizeCheckConfig.messageSize;
            response.result(http::status::bad_request);
            return;
        }
    }

    Account destination;
    if (bSyncMessage) {
        destination = source;
    } else {
        http::status result;
        getDestination(destinationUid, destination, result);
        if (result != http::status::ok) {
            MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsMessageServiceName,
                "sendMessage", (nowInMicro() - dwStartTime), 1002);
            response.result(result);
            return;
        }
    }

    // accept stranger and bloom filters
    bool filterStrangerMsg = false;
    if (destination.has_privacy()) {
        if (!destination.privacy().acceptstrangermsg() && destination.has_contactsfilters()) {
            filterStrangerMsg = true;
        }
    }

    if (filterStrangerMsg && !bSyncMessage) {
        BloomFilters bloomFilters{destination.contactsfilters().algo(), Base64::decode(destination.contactsfilters().content())};
        if (!bloomFilters.contains(source.uid())) {
            MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsMessageServiceName,
                    "sendMessage", (nowInMicro() - dwStartTime), 1404);
            LOGI << "source uid blocked by destination: " << source.uid() << " -> " << destinationUid;
            response.result(http::status::ok);
            context.responseEntity = SendMessageResponse(false);
            return;
        }
    }

    MismatchedDevices mismatchedDevices;
    if (!validateCompleteDeviceList(destination, messageList, bSyncMessage, mismatchedDevices)) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsMessageServiceName,
            "sendMessage", (nowInMicro() - dwStartTime), 1003);
        if (m_multiDeviceConfig.needUpgrade) {
            auto authDevice = *AccountsManager::getAuthDevice(source);
            auto& clientVer = authDevice.clientversion();
            std::string feature_string = AccountsManager::getFeatures(source, authDevice.id());
            BcmFeatures features(HexEncoder::decode(feature_string));
            if (clientVer.ostype() == ClientVersion::OSTYPE_IOS  || clientVer.ostype() == ClientVersion::OSTYPE_ANDROID) {
                if(!features.hasFeature(bcm::Feature::FEATURE_MULTI_DEVICE)) {
                    response.result(static_cast<unsigned>(custom_http_status::upgrade_requried));
                    return;
                }
            }
        }
        handleJsonError(http::status::conflict, mismatchedDevices, JsonSerializerImp<MismatchedDevices>{});
        return;
    }

    StaleDevices staleDevices;
    if (!validateRegistrationIds(destination, messageList, staleDevices)) {
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsMessageServiceName,
            "sendMessage", (nowInMicro() - dwStartTime), 1004);
        handleJsonError(http::status::gone, staleDevices, JsonSerializerImp<StaleDevices>{});
        return;
    }

    // encrypt source
    // add support for multi device
    std::map<uint32_t, std::string> sourceExtraMap;
    for (auto& message : messageList.messages) {
        auto& device = *AccountsManager::getDevice(destination, message.destinationDeviceId);
        std::string sourceExtra = generateSourceExtra(source.uid(), destination, device);
        if ("" == sourceExtra) {
            MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsMessageServiceName,
                    "sendMessage", (nowInMicro() - dwStartTime), 1005);
            LOGE << "cannot encrypt source: " << source.uid() << ": " << destination.uid() << ": " << device.id();

            response.result(http::status::internal_server_error);
            return;
        }
        LOGD << "generate sourceExtra from uid: " << source.uid() << " to sourceExtra: " << sourceExtra;
        sourceExtraMap[device.id()] = std::move(sourceExtra);
    }

    for (auto& message : messageList.messages) {
        auto& device = *AccountsManager::getDevice(destination, message.destinationDeviceId);
        sendLocalMessage(source, destination, device, messageList.timestamp, message, sourceExtraMap[device.id()]);
    }

    context.responseEntity = SendMessageResponse(!bSyncMessage && (source.devices_size() > 1));
    response.result(http::status::ok);

    MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsMessageServiceName,
        "sendMessage", (nowInMicro() - dwStartTime), 0);
}

bool MessageController::sendLocalMessage(const Account& source, const Account& destination,
                                         const Device& destinationDevice, uint64_t timestamp,
                                         const IncomingMessage& message, const std::string& sourceExtra)
{
    auto envelope = createEnvelope(source, timestamp, message, sourceExtra);

    PubSubMessage pubMessage;
    pubMessage.set_type(PubSubMessage::DELIVER);
    pubMessage.set_content(envelope.SerializeAsString());

    auto address = DispatchAddress(destination.uid(), destinationDevice.id());
    if (m_dispatchManager->publish(address, pubMessage.SerializeAsString())) {
        LOGD << "success to publish message to : " << address;
        return true;
    }

    // ignore noise message when destination is offline
    if (message.type == Envelope::NOISE) {
        return true;
    }
    LOGD << "store message and push notification since peer offline: " << source.uid() << "->" << address;

    uint32_t storedMessagesCount = 0;
    auto ret = m_dispatchManager->getMessagesManager().store(destination.uid(), destinationDevice.id(),
                                                             destinationDevice.registrationid(), envelope,
                                                             storedMessagesCount);

    if (!ret) {
        return false;
    }

    // slave device need not offline push
    if (destinationDevice.id() != Device::MASTER_ID) {
        return true;
    }

    // send to self need not offline push
    if (source.uid() == destination.uid()) {
        return true;
    }

    if (envelope.type() != Envelope::RECEIPT
            && static_cast<push::Classes>(envelope.push()) != push::Classes::SILENT
            && AccountsManager::isDevicePushable(destinationDevice)) {
        std::string sourceInPushService = SenderUtils::getSourceInPushService(destinationDevice,
                                                                              envelope,
                                                                              m_encryptSenderConfig);
        push::Notification notification;
        notification.chat(sourceInPushService);
        notification.setBadge(storedMessagesCount);
        notification.setTargetAddress(address);
        notification.setDeviceInfo(destinationDevice);
        notification.setClass(static_cast<push::Classes>(envelope.push()));

        std::string pushType = notification.getPushType();
        if (!pushType.empty()) {
            m_offlineDispatcher->dispatch(pushType, notification);
        }
    }

    return true;
}

std::string MessageController::generateSourceExtra(const std::string& source, const Account& destination, const Device& device)
{
    uint32_t version;
    std::string iv;
    std::string ephemeralPubkey;
    std::string encryptedSource;
    int ret = 0;

    std::string destinationPublicKey;
    if (device.publickey().empty()) {
        if (device.id() == Device::MASTER_ID) {
            destinationPublicKey = destination.publickey();
        } else {
            return "";
        }
    } else {
        destinationPublicKey = device.publickey();
    }
    if ((ret = SenderUtils::encryptSender(source, destinationPublicKey,
                                          version, iv, ephemeralPubkey, encryptedSource)) != 0) {
        LOGE << "encrypt sender failed, result code : " << ret
             << ", sender : " << source << ", destination public key : " << destination.publickey();
        return "";
    }
    nlohmann::json eFrom = nlohmann::json::object({
            {"version",         version},
            {"ephemeralPubkey", Base64::encode(ephemeralPubkey)},
            {"iv",              Base64::encode(iv)},
            {"source",          Base64::encode(encryptedSource)}
    });
    return Base64::encode(eFrom.dump());
}

Envelope MessageController::createEnvelope(const Account& source, uint64_t timestamp,
                                           const IncomingMessage& message, const std::string& sourceExtra)
{
    Envelope envelope;

    envelope.set_type(static_cast<Envelope::Type>(message.type));
    envelope.set_timestamp((timestamp == 0) ? nowInMilli() : timestamp);
    envelope.set_source(m_encryptSenderConfig.plainUidSupport ? source.uid() : "");
    envelope.set_relay(""); //TODO: source relay is not support now.
    envelope.set_sourcedevice(source.authdeviceid());
    envelope.set_sourceregistration(AccountsManager::getAuthDevice(source)->registrationid());
    envelope.set_sourceextra(sourceExtra);
    envelope.set_push(message.push);

    auto decodedBody = Base64::decode(message.body);
    if (!decodedBody.empty()) {
        envelope.set_legacymessage(decodedBody);
    }

    auto decodedContent = Base64::decode(message.content);
    if (!decodedContent.empty()) {
        envelope.set_content(decodedContent);
    }

    return envelope;
}

void MessageController::getDestination(const std::string& destinationUid, Account& destination, http::status& result)
{
    dao::ErrorCode ret = m_accountsManager->get(destinationUid, destination);
    if (ret == dao::ERRORCODE_NO_SUCH_DATA) {
        LOGT << "no such user, uid: " << destinationUid;
        result = http::status::not_found;
        return;
    }

    if (ret != dao::ERRORCODE_SUCCESS) {
        result = http::status::internal_server_error;
        LOGE << "failed to get destination: " << destinationUid << ", error:" << ret;
        return;
    }

    // TODO: return 410 GONE? this status code is used to indicate stale registration ids
    if (destination.state() == Account::DELETED) {
        LOGD << "account is destoryed, uid: " << destinationUid;
        result = http::status::not_found;
        return;
    }

    if (!AccountsManager::isAccountActive(destination)) {
        LOGD << "account is not active, uid: " << destinationUid;
        result = http::status::not_found;
        return;
    }
    result = http::status::ok;
}

bool MessageController::validateCompleteDeviceList(const Account& destination, const IncomingMessageList& messageList,
                                                   bool isSyncMessage, MismatchedDevices& mismatchedDevices)
{
    std::set<uint32_t> messageDeviceIds;
    std::set<uint32_t> accountDeviceIds;

    // for noise message, the target should be the sender himself
    if (messageList.messages.size() == 1) {
        const auto& msg = messageList.messages.front();
        if (msg.type == Envelope::NOISE) {
            return isSyncMessage && msg.destinationDeviceId == destination.authdeviceid();
        }
    }

    for (const auto& message : messageList.messages) {
        messageDeviceIds.insert(message.destinationDeviceId);
    }

    for (auto& device : destination.devices()) {
        if (!AccountsManager::isDeviceActive(device)) {
            continue;
        }

        // sync message can not send to self device
        if (isSyncMessage && (device.id() == destination.authdeviceid())) {
            continue;
        }

        // each message should be sent to all active devices of destination,
        // so that message list should not miss any device id
        if (messageDeviceIds.count(device.id()) == 0) {
            LOGW << "miss device: " << device.id();
            mismatchedDevices.missingDevices.push_back(device.id());
        }

        accountDeviceIds.insert(device.id());
    }

    for (auto& id : messageDeviceIds) {
        // message can not send to an absent device
        if (accountDeviceIds.count(id) == 0) {
            LOGW << "absent device: " << id;
            mismatchedDevices.extraDevices.push_back(id);
        }
    }

    return mismatchedDevices.missingDevices.empty() && mismatchedDevices.extraDevices.empty();
}

bool MessageController::validateRegistrationIds(const Account& destination, const IncomingMessageList& messageList,
                                                StaleDevices& staleDevices)
{
    for (const auto& message : messageList.messages) {
        auto device = AccountsManager::getDevice(destination, message.destinationDeviceId);
        // validate registration id if message set it lager than 0
        if (device != boost::none && message.destinationRegistrationId > 0
                && static_cast<uint32_t>(message.destinationRegistrationId) != device->registrationid()) {
            LOGW << "registration id is mismatched: "
                 << message.destinationRegistrationId << " != " << device->registrationid()
                 << ", uid: " << destination.uid() << ", device: " << device->id();
            staleDevices.staleDevices.push_back(device->id());
        }
    }

    return staleDevices.staleDevices.empty();
}

}
