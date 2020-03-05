#include <boost/asio.hpp>
#include "utils/log.h"
#include "apns_notification.h"
#include "apns_service.h"

namespace bcm {
namespace push {
namespace apns {
// -----------------------------------------------------------------------------
// Section: ServiceImpl
// -----------------------------------------------------------------------------
    
static std::string  VOIP_FUFFIX = "_voip";
    
class ServiceImpl {
    std::map<std::string, std::unique_ptr<Client>> m_typeMap;
    Client* m_defaultClient;
    Client* m_defaultVoipClient;
    int32_t m_expirySecs;

public:
    ServiceImpl() : m_defaultClient(nullptr), m_defaultVoipClient(nullptr), m_expirySecs(30) {}

    bool init(const ApnsConfig& apns) noexcept
    {
        m_expirySecs = apns.expirySecs;
        LOGI << "apns expiry time is set to " << m_expirySecs << " secs";
        
        std::string  printDefaultType;
        for (const auto& cfg : apns.entries) {
            std::unique_ptr<Client> cli = std::make_unique<Client>();
            cli->bundleId(cfg.bundleId);
            if (cfg.sandbox) {
                cli->development();
            } else {
                cli->production();
            }
            cli->certificateFile(cfg.certFile);
            cli->privateKeyFile(cfg.keyFile);
            if (cfg.defaultSender) {
                
                if (cfg.type.find(VOIP_FUFFIX) != std::string::npos) {
                    printDefaultType += ", voip type: " + cfg.type;
                    m_defaultVoipClient = cli.get();
                } else {
                    printDefaultType += ", type: " + cfg.type;
                    m_defaultClient = cli.get();
                }
            }
            cli->setType(cfg.type);
            m_typeMap.emplace(cfg.type, std::move(cli));
            LOGI << "add apns client: type '" << cfg.type << "' env '"
                 << (cfg.sandbox ? "development" : "production") << "'";
        }
        
        if (m_defaultClient == nullptr || m_defaultVoipClient == nullptr) {
            LOGE << "add default client " << printDefaultType;
            return false;
        }
        return true;
    }

    const std::string& bundleId(const std::string& type, const bool isVoip = false) const noexcept
    {
        static const std::string kInvalidBundleId;
        
        std::string  typeKey;
        if (isVoip) {
            typeKey = type + VOIP_FUFFIX;
        } else {
            typeKey = type;
        }
        
        auto iter = m_typeMap.find(typeKey);
        if (iter != m_typeMap.end()) {
            return iter->second->bundleId();
        } else {
            if (isVoip && m_defaultVoipClient) {
                return m_defaultVoipClient->bundleId();
            }
            
            if ((!isVoip) && m_defaultClient) {
                return m_defaultClient->bundleId();
            }
        }
        return kInvalidBundleId;
    }

    int32_t expirySecs() const noexcept
    {
        return m_expirySecs;
    }

    SendResult send(const std::string& type,
                    const Notification& notification) noexcept
    {
        std::string  typeKey;
        if (notification.isVoip()) {
            typeKey = type + VOIP_FUFFIX;
        } else {
            typeKey = type;
        }
        
        auto iter = m_typeMap.find(typeKey);
        if (iter == m_typeMap.end()) {
            SendResult result;
            result.ec = boost::asio::error::operation_not_supported;
            return result;
        }
        
        LOGD << "send apns notification: apns type: " << type
             << ", token: " << notification.token()
             << ", topic: " << notification.topic()
             << ", notification: " << notification.toString();
        return iter->second->send(notification);
    }
};

// -----------------------------------------------------------------------------
// Section: Service
// -----------------------------------------------------------------------------
Service::Service() : m_pImpl(new ServiceImpl()), m_impl(*m_pImpl) {}

Service::~Service()
{
    if (m_pImpl != nullptr) {
        delete m_pImpl;
    }
}

bool Service::init(const ApnsConfig& apns) noexcept
{
    return m_impl.init(apns);
}

const std::string& Service::bundleId(const std::string& type, const bool isVoip) const noexcept
{
    return m_impl.bundleId(type, isVoip);
}

int32_t Service::expirySecs() const noexcept
{
    return m_impl.expirySecs();
}

SendResult Service::send(const std::string& type,
                         const Notification& notification) noexcept
{
    return m_impl.send(type, notification);
}

} // namespace apns
} // namespace push
} // namespace bcm