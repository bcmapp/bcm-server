#pragma once

#include <string>
#include <vector>
#include <chrono>
#include "nlohmann/json.hpp"

namespace bcm {
namespace push {
namespace umeng {

enum class BroadcastType {
    UNICAST = 1,        // send to single device
    LISTCAST,           // send to multiple devices with 500 limit
    GROUPCAST           // send to group
};

enum class DisplayType {
    NOTIFICATION = 1,
    MESSAGE,
};

enum class AfterOpenAction {
    OPEN_APP = 1,
    OPEN_URL,
    START_ACTIVITY,
    CUSTOM,
};

// -----------------------------------------------------------------------------
// Section: Notification
// -----------------------------------------------------------------------------
class Notification {
    nlohmann::json m_j;
    nlohmann::json& m_payload;
    nlohmann::json& m_payloadBody;

public:
    Notification();
    Notification(const Notification& other) = default;
    Notification(Notification&& other) = default;
    Notification& operator=(const Notification& other) = default;
    Notification& operator=(Notification&& other) = default;

    Notification& appKey(const std::string& key);
    Notification& timestamp(std::time_t t);
    Notification& deviceToken(const std::string& token);
    Notification& deviceToken(const std::vector<std::string>& tokens);
    Notification& filter(const std::string& topic);
    Notification& displayType(DisplayType type);
    Notification& ticker(const std::string& ticker);
    Notification& title(const std::string& title);
    Notification& text(const std::string& text);
    Notification& icon(const std::string& icon);
    Notification& largeIcon(const std::string& icon);
    Notification& image(const std::string& image);
    Notification& sound(const std::string& sound);
    Notification& playVibrate(bool isPlay);
    Notification& playLights(bool isPlay);
    Notification& playSound(bool isPlay);
    Notification& openApp();
    Notification& openUrl(const std::string& url);
    Notification& startActivity(const std::string& activity);
    Notification& customAction(const std::string& action);
    Notification& customAction(const nlohmann::json& action);
    Notification& extra(const std::string& key, const std::string& value);
    Notification& extra(const std::string& key, const nlohmann::json& value);
    Notification& startTime(const std::chrono::system_clock::time_point& t);
    Notification& expiryTime(const std::chrono::system_clock::time_point& t);
    Notification& productionMode(bool isProduction);
    Notification& description(const std::string& desc);
    Notification& miPush(bool isMiPush);
    Notification& miActivity(const std::string& activity);

    std::string serialize() const;
};

} // namespace umeng
} // namespace push
} // namespace bcm
