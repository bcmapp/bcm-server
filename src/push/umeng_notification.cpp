#include <chrono>
#include <iomanip>
#include <ctime>
#include <boost/algorithm/string/join.hpp>
#include "umeng_notification.h"

namespace bcm {
namespace push {
namespace umeng {

using system_clock = std::chrono::system_clock;

static const std::string kInvalidValue;

static const std::string& asString(BroadcastType type)
{
    static const std::string kUniCast = "unicast";
    static const std::string kListCast = "listcast";
    static const std::string kGroupCast = "groupcast";
    switch (type) {
    case BroadcastType::UNICAST:
        return kUniCast;
    case BroadcastType::LISTCAST:
        return kListCast;
    case BroadcastType::GROUPCAST:
        return kGroupCast;
    }
    return kInvalidValue;
}

static const std::string& asString(DisplayType type)
{
    static const std::string kNotification = "notification";
    static const std::string kMessage = "message";
    switch (type) {
    case DisplayType::NOTIFICATION:
        return kNotification;
    case DisplayType::MESSAGE:
        return kMessage;
    }
    return kInvalidValue;
}

static const std::string& asString(AfterOpenAction action)
{
    static const std::string kGoApp = "go_app";
    static const std::string kGoUrl = "go_url";
    static const std::string kGoActivity = "go_activity";
    static const std::string kGoCustom = "go_custom";
    switch (action) {
    case AfterOpenAction::OPEN_APP:
        return kGoApp;
    case AfterOpenAction::OPEN_URL:
        return kGoUrl;
    case AfterOpenAction::START_ACTIVITY:
        return kGoActivity;
    case AfterOpenAction::CUSTOM:
        return kGoCustom;
    }
    return kInvalidValue;
}

static std::string asString(const system_clock::time_point& t)
{
    char buf[100];
    std::time_t tt = system_clock::to_time_t(t);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&tt));
    return std::string(buf);
}

// -----------------------------------------------------------------------------
// Section: Notification
// -----------------------------------------------------------------------------
Notification::Notification()
    : m_payload(m_j["payload"])
    , m_payloadBody(m_payload["body"])
{
    timestamp(system_clock::to_time_t(system_clock::now()));
}

Notification& Notification::appKey(const std::string& key)
{
    m_j["appkey"] = key;
    return *this;
}

Notification& Notification::timestamp(std::time_t t)
{
    m_j["timestamp"] = std::to_string(t);
    return *this;
}

Notification& Notification::deviceToken(const std::string& token)
{
    m_j["type"] = asString(BroadcastType::UNICAST);
    m_j["device_tokens"] = token;
    return *this;
}

Notification& Notification::deviceToken(const std::vector<std::string>& tokens)
{
    m_j["type"] = asString(BroadcastType::LISTCAST);
    m_j["device_tokens"] = boost::algorithm::join(tokens, ",");
    return *this;
}

Notification& Notification::filter(const std::string& topic)
{
    nlohmann::json conditionExp;
    conditionExp["tag"] = topic;
    nlohmann::json orConditionExp;
    orConditionExp["or"] = {conditionExp,};
    m_j["type"] = asString(BroadcastType::GROUPCAST);
    m_j["filter"]["where"]["and"] = {orConditionExp,};
    return *this;
}

Notification& Notification::displayType(DisplayType type)
{
    m_payload["display_type"] = asString(type);
    return *this;
}

Notification& Notification::ticker(const std::string& ticker)
{
    m_payloadBody["ticker"] = ticker;
    return *this;
}

Notification& Notification::title(const std::string& title)
{
    m_payloadBody["title"] = title;
    return *this;
}

Notification& Notification::text(const std::string& text)
{
    m_payloadBody["text"] = text;
    return *this;
}

Notification& Notification::icon(const std::string& icon)
{
    m_payloadBody["icon"] = icon;
    return *this;
}

Notification& Notification::largeIcon(const std::string& icon)
{
    m_payloadBody["largeIcon"] = icon;
    return *this;
}

Notification& Notification::image(const std::string& image)
{
    m_payloadBody["img"] = image;
    return *this;
}

Notification& Notification::sound(const std::string& sound)
{
    m_payloadBody["sound"] = sound;
    return *this;
}

Notification& Notification::playVibrate(bool isPlay)
{
    m_payloadBody["play_vibrate"] = isPlay ? "true" : "false";
    return *this;
}

Notification& Notification::playLights(bool isPlay)
{
    m_payloadBody["play_lights"] = isPlay ? "true" : "false";
    return *this;
}

Notification& Notification::playSound(bool isPlay)
{
    m_payloadBody["play_sound"] = isPlay ? "true" : "false";
    return *this;
}

Notification& Notification::openApp()
{
    m_payloadBody["after_open"] = asString(AfterOpenAction::OPEN_APP);
    m_payloadBody.erase("url");
    m_payloadBody.erase("activity");
    m_payloadBody.erase("custom");
    return *this;
}

Notification& Notification::openUrl(const std::string& url)
{
    m_payloadBody["after_open"] = asString(AfterOpenAction::OPEN_URL);
    m_payloadBody["url"] = url;
    m_payloadBody.erase("activity");
    m_payloadBody.erase("custom");
    return *this;
}

Notification& Notification::startActivity(const std::string& activity)
{
    m_payloadBody["after_open"] = asString(AfterOpenAction::START_ACTIVITY);
    m_payloadBody["activity"] = activity;
    m_payloadBody.erase("url");
    m_payloadBody.erase("custom");
    return *this;
}

Notification& Notification::customAction(const std::string& action)
{
    m_payloadBody["after_open"] = asString(AfterOpenAction::CUSTOM);
    m_payloadBody["custom"] = action;
    m_payloadBody.erase("url");
    m_payloadBody.erase("activity");
    return *this;
}

Notification& Notification::customAction(const nlohmann::json& action)
{
    m_payloadBody["after_open"] = asString(AfterOpenAction::CUSTOM);
    m_payloadBody["custom"] = action;
    m_payloadBody.erase("url");
    m_payloadBody.erase("activity");
    return *this;
}

Notification& Notification::extra(const std::string& key, 
                                  const std::string& value)
{
    m_payload["extra"][key] = value;
    return *this;
}

Notification& Notification::extra(const std::string& key,
                                  const nlohmann::json& value)
{
    m_payload["extra"][key] = value;
    return *this;
}

Notification& Notification::startTime(const system_clock::time_point& t)
{
    m_j["policy"]["start_time"] = asString(t);
    return *this;
}

Notification& Notification::expiryTime(const system_clock::time_point& t)
{
    m_j["policy"]["expire_time"] = asString(t);
    return *this;
}

Notification& Notification::productionMode(bool isProduction)
{
    m_j["production_mode"] = isProduction ? "true" : "false";
    return *this;
}

Notification& Notification::description(const std::string& desc)
{
    m_j["description"] = desc;
    return *this;
}

Notification& Notification::miPush(bool isMiPush)
{
    m_j["mipush"] = isMiPush;
    return *this;
}

Notification& Notification::miActivity(const std::string& activity)
{
    m_j["mi_activity"] = activity;
    return *this;
}

std::string Notification::serialize() const
{
    return m_j.dump();
}

} // namespace umeng
} // namespace push
} // namespace bcm