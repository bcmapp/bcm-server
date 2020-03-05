#include "nlohmann/json.hpp"
#include "fcm_notification.h"

namespace bcm {
namespace push {
namespace fcm {

static const std::string kInvalidValue;
static const std::string kTopicPrefix = "/topics/";

static const std::string& asString(Priority priority)
{
    static const std::string kNormal = "normal";
    static const std::string kHigh = "high";
    switch (priority) {
    case Priority::NORMAL:
        return kNormal;
    case Priority::HIGH:
        return kHigh;
    }
    return kInvalidValue;
}

Notification::Notification()
{
    // default setting
    m_j["notification"]["sound"] = "default";
}

Notification& Notification::collapseKey(const std::string& key)
{
    m_j["collapse_key"] = key;
    return *this;
}

Notification& Notification::ttl(int32_t secs)
{
    m_j["time_to_live"] = secs;
    return *this;
}

Notification& Notification::delayWhileIdle(bool isDelayWhileIdle)
{
    m_j["delay_while_idle"] = isDelayWhileIdle;
    return *this;
}
Notification& Notification::topic(const std::string& topic)
{
    m_j["to"] = kTopicPrefix + topic;
    m_j.erase("registration_ids");
    return *this;
}

Notification& Notification::addDataPart(const std::string& key,
                                        const std::string& value)
{
    m_j["data"][key] = value;
    return *this;
}

Notification& Notification::destination(const std::string& registrationId)
{
    m_j["to"] = registrationId;
    m_j.erase("registration_ids");
    return *this;
}

Notification& Notification::destination(
    const std::vector<std::string>& registrationIds)
{
    m_j["registration_ids"] = registrationIds;
    m_j.erase("to");
    return *this;
}

Notification& Notification::priority(Priority priority)
{
    m_j["priority"] = asString(priority);
    return *this;
}

Notification& Notification::title(const std::string& title)
{
    m_j["notification"]["title"] = title;
    return *this;
}

Notification& Notification::text(const std::string& text)
{
    m_j["notification"]["body"] = text;
    return *this;
}

std::string Notification::serialize() const
{
    return m_j.dump();
}

} // namespace fcm
} // namespace push
} // namespace bcm
