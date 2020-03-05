#pragma once

#include <string>
#include <map>
#include <vector>
#include "nlohmann/json.hpp"

namespace bcm {
namespace push {
namespace fcm {

enum class Priority {
    NORMAL = 1,
    HIGH,
};

class Notification {
    nlohmann::json m_j;

public:
    Notification();
    Notification(const Notification& other) = default;
    Notification(Notification&& other) = default;
    Notification& operator=(const Notification& other) = default;
    Notification& operator=(Notification&& other) = default;

    Notification& collapseKey(const std::string& key);
    Notification& ttl(int32_t secs);
    Notification& delayWhileIdle(bool isDelayWhileIdle);
    Notification& addDataPart(const std::string& key, const std::string& value);
    Notification& destination(const std::string& registrationId);
    Notification& topic(const std::string& topic);
    Notification& destination(const std::vector<std::string>& registrationIds);
    Notification& priority(Priority priority);
    Notification& title(const std::string& title);
    Notification& text(const std::string& text);

    std::string serialize() const;
};


} // namespace fcm
} // namespace push
} // namespace bcm
