#pragma once

#include "push/push_notification.h"

#include <nlohmann/json.hpp>
#include <nghttp2/asio_http2.h>

#include <chrono>

namespace bcm {
namespace push {
namespace apns {
// -----------------------------------------------------------------------------
// Section: Aps
// -----------------------------------------------------------------------------
struct Aps {
    // Include this key when you want the system to modify the badge of your
    // app icon. If this key is not included in the dictionary, the badge is
    // not changed. To remove the badge, set the value of this key to 0.
    // NOTE: a negative value will exclude this key from the dictionary.
    int badge;

    // Include this key when you want the system to play a sound. The value of
    // this key is the name of a sound file in your app’s main bundle or in the
    // Library/Sounds folder of your app’s data container. If the sound file
    // cannot be found, or if you specify default for the value, the system
    // plays the default alert sound.
    std::string sound;

    // The following keys are not supported yet:
    // alert
    // content-available
    // category
    // thread-id
    Aps() noexcept;

    Aps(const Aps& other) = default;
    Aps(Aps&& other) = default;
    Aps& operator=(const Aps& other) = default;
    Aps& operator=(Aps&& other) = default;
};

void to_json(nlohmann::json& j, const Aps& aps);

// -----------------------------------------------------------------------------
// Section: Notification
// -----------------------------------------------------------------------------
enum DeliveryPriority {
    // Send the push message immediately. Notifications with this priority
    // must trigger an alert, sound, or badge on the target device. It is an
    // error to use this priority for a push notification that contains only
    // the content-available key.
    IMMEDIATE = 10,

    // Send the push message at a time that takes into account power
    // considerations for the device. Notifications with this priority might
    // be grouped and delivered in bursts. They are throttled, and in some
    // cases are not delivered.
    CONSERVE_POWER = 5,
};

class Notification {
public:
    virtual ~Notification() { }

    virtual void copy(const Notification& other) = 0;
    virtual void copy(Notification&& other) = 0;
    virtual Notification* clone() const = 0;
    virtual std::string toString() const = 0;

    // Returns the token of the device to which this push notification is to
    // be sent.
    //
    // A canonical UUID that identifies the notification. If there is an error
    // sending the notification, APNs uses this value to identify the
    // notification to your server. The canonical form is 32 lowercase
    // hexadecimal digits, displayed in five groups separated by hyphens in
    // the form 8-4-4-4-12. An example UUID is as follows:
    //      123e4567-e89b-12d3-a456-42665544000
    //
    // If you omit this header, a new UUID is created by APNs and returned in
    // the response.
    virtual std::string token() const = 0;

    // Returns the JSON-encoded payload of this push notification.
    virtual std::string payload() const = 0;

    // Returns the time at which Apple's push notification service should stop
    // trying to deliver this push notification.
    //
    // A UNIX epoch date expressed in seconds (UTC), If this value is nonzero, 
    // APNs stores the notification and tries to deliver it at least once, 
    // repeating the attempt as needed if it is unable to deliver the 
    // notification the first time. If the value is 0, APNs treats the 
    // notification as if it expires immediately and does not store the 
    // notification or attempt to redeliver it.
    virtual int64_t expiryTime() const { return 0; }

    // Returns the priority with which this push notification should be sent
    // to the receiving device.
    virtual DeliveryPriority priority() const { return IMMEDIATE; }

    // Returns the topic to which this notification should be sent. This is
    // generally the bundle ID of the receiving app.
    virtual std::string topic() const = 0;

    // Returns an optional identifier for this notification that allows this
    // notification to supersede previous notifications or to be superseded by
    // later notifications with the same identifier.
    virtual std::string collapseId() const { return ""; }

    virtual bool isVoip() const = 0;
};

// -----------------------------------------------------------------------------
// Section: SimpleNotification
// -----------------------------------------------------------------------------
class SimpleNotification : public Notification {
public:
    SimpleNotification();
    virtual ~SimpleNotification() { }

    SimpleNotification(const SimpleNotification& other) = default;
    SimpleNotification(SimpleNotification&& other) = default;
    SimpleNotification& operator=(const SimpleNotification& other) = default;
    SimpleNotification& operator=(SimpleNotification&& other) = default;

    void copy(const Notification& other) override;
    void copy(Notification&& other) override;
    Notification* clone() const override;
    std::string toString() const override;
    std::string token() const override;
    std::string payload() const override;
    int64_t expiryTime() const override;
    std::string topic() const override;
    std::string collapseId() const override;
    bool isVoip() const override;

public:
    SimpleNotification& bundleId(const std::string& bundleId);
    SimpleNotification& apnId(const std::string& apnId);
    SimpleNotification& voip(bool isVoip);
    SimpleNotification& expiryTime(
                            const std::chrono::system_clock::duration& t);
    SimpleNotification& expiryTime(int32_t secs);

    SimpleNotification& collapseId(const std::string& collapseId);
    SimpleNotification& badge(int badge);
    SimpleNotification& sound(const std::string& sound);

    SimpleNotification& data(const nlohmann::json& data);

private:
    std::string m_bundleId;
    std::string m_apnId;
    bool m_isVoip;
    int64_t m_expiryTime;
    std::string m_collapseId;
    Aps m_aps;
    nlohmann::json m_data;
};

} // namespace apns
} // namespace push
} // namespace bcm
