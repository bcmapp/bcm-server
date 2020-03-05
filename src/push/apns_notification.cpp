#include "apns_notification.h"

using namespace nlohmann;

namespace bcm {
namespace push {
namespace apns {
// -----------------------------------------------------------------------------
// Section: Aps
// -----------------------------------------------------------------------------
Aps::Aps() noexcept : badge(-1), sound("default") {}

void to_json(nlohmann::json& j, const Aps& aps)
{
    if (aps.badge >= 0) {
        j["badge"] = aps.badge;
    }
    j["sound"] = aps.sound;
    j["alert"]["loc-key"] = "APN_Message";
    j["mutable-content"] = 1;
}

// -----------------------------------------------------------------------------
// Section: Message
// -----------------------------------------------------------------------------
typedef nghttp2::asio_http2::header_map header_map_t;
typedef nghttp2::asio_http2::header_value header_value_t;

SimpleNotification::SimpleNotification() : m_isVoip(false), m_expiryTime(0) {}

void SimpleNotification::copy(const Notification& other)
{
    this->operator=(static_cast<const SimpleNotification&>(other));
}

void SimpleNotification::copy(Notification&& other)
{
    this->operator=(static_cast<SimpleNotification&&>(other));
}

Notification* SimpleNotification::clone() const
{
    SimpleNotification* n = new SimpleNotification(*this);
    return n;
}

std::string SimpleNotification::toString() const
{
    std::stringstream ss;
    ss << ", token: " << token() << ", payload: " << payload() 
       << ", expirtyTime: " << expiryTime() << ", topic: " << topic() 
       << ", collapseId: " << collapseId();
    return ss.str();
}

std::string SimpleNotification::token() const
{
    return m_apnId;
}

std::string SimpleNotification::payload() const
{
    nlohmann::json j;
    j["aps"] = m_aps;
    j["bcmdata"] = m_data;
    return j.dump();
}

int64_t SimpleNotification::expiryTime() const
{
    return m_expiryTime;
}

std::string SimpleNotification::topic() const
{
    if (m_isVoip) {
        return m_bundleId + ".voip";
    } else {
        return m_bundleId;
    }
}

std::string SimpleNotification::collapseId() const
{
    return m_collapseId;
}

SimpleNotification& SimpleNotification::bundleId(const std::string& bundleId)
{
    m_bundleId = bundleId;
    return *this;
}

SimpleNotification& SimpleNotification::apnId(const std::string& apnId)
{
    m_apnId = apnId;
    return *this;
}

SimpleNotification& SimpleNotification::voip(bool isVoip)
{
    m_isVoip = isVoip;
    return *this;
}

bool SimpleNotification::isVoip() const
{
    return m_isVoip;
}

SimpleNotification& SimpleNotification::expiryTime(
                        const std::chrono::system_clock::duration& t)
{
    m_expiryTime = std::chrono::duration_cast<std::chrono::seconds>(t).count();
    return *this;
}

SimpleNotification& SimpleNotification::expiryTime(int32_t secs)
{
    if (secs != 0) {
        return expiryTime(std::chrono::system_clock::now().time_since_epoch() 
                            + std::chrono::seconds(secs));
    } else {
        m_expiryTime = 0;
    }
    return *this;
}

SimpleNotification& SimpleNotification::collapseId(
                                            const std::string& collapseId)
{
    m_collapseId = collapseId;
    return *this;
}

SimpleNotification& SimpleNotification::badge(int badge)
{
    m_aps.badge = badge;
    return *this;
}

SimpleNotification& SimpleNotification::sound(const std::string& sound)
{
    m_aps.sound = sound;
    return *this;
}

SimpleNotification& SimpleNotification::data(const nlohmann::json& data)
{
    m_data = data;
    return *this;
}

} // namespace apns
} // namespace push
} // namespace bcm