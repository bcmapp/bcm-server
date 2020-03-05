#pragma once

#include <string>
#include <boost/system/error_code.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace bcm {
namespace push {
namespace apns {
class Notification;

// -----------------------------------------------------------------------------
// Section: SendResult
// -----------------------------------------------------------------------------
struct SendResult {
    boost::system::error_code ec;

    // The HTTP status code, see
    // https://developer.apple.com/library/archive/documentation/
    // NetworkingInternet/Conceptual/RemoteNotificationsPG/
    // CommunicatingwithAPNs.html for detail
    int statusCode;

    // If the value in the |statusCode| is 410, the value of this field is the
    // last time at which APNs confirmed that the device token was no longer
    // valid for the topic. Stop pushing notifications until the device
    // registers a token with a later timestamp with your provider.
    int64_t timestamp;

    // The error indicating the reason for the failure. The error code
    // is specified as a string, see
    // https://developer.apple.com/library/archive/documentation/
    // NetworkingInternet/Conceptual/RemoteNotificationsPG/
    // CommunicatingwithAPNs.html for detail
    std::string error;

    // The apns-id value from the request. If no value was included in the
    // request, the server creates a new UUID and returns it in this field
    std::string apnsId;

    explicit SendResult(const boost::system::error_code& ec);

    SendResult() = default;
    SendResult(const SendResult& other) = default;
    SendResult(SendResult&& other) = default;
    SendResult& operator=(const SendResult& other) = default;
    SendResult& operator=(SendResult&& other) = default;

    bool isUnregistered() const;
};

class ClientImpl;
// -----------------------------------------------------------------------------
// Section: Client
// -----------------------------------------------------------------------------
class Client : private boost::noncopyable {
    typedef boost::system::error_code error_code;
    typedef boost::posix_time::time_duration time_duration;

public:
    Client();
    ~Client();

    Client& bundleId(const std::string& bundleId) noexcept;
    Client& production() noexcept;
    Client& development() noexcept;

    // NOTE: the certificate file must be PEM format
    Client& certificateFile(const std::string& path);

    // NOTE: the private key file must be PEM format
    Client& privateKeyFile(const std::string& path);

    // Default connect timeout is 60 seconds
    Client& connectTimeout(const time_duration& timeout) noexcept;

    // Default read timeout is 60 seconds.
    // WARNING: read time must be greater than or equal to 30 seconds, as
    // nghttp2 pings server every 30 seconds. If read time out is less than 30
    // seconds, the read timer will be timed out before data is available to be
    // read.
    Client& readTimeout(const time_duration& timeout) noexcept;

    const std::string& bundleId() const noexcept;
    void setType(const std::string& type) noexcept;
    
    void start() noexcept;
    SendResult send(const Notification& notification) noexcept;

#ifdef APNS_TEST
    void start(boost::system::error_code& ec) noexcept;
    void restart(boost::system::error_code& ec) noexcept;
    void shutdown() noexcept;
#endif

private:
    ClientImpl* m_pImpl;
    ClientImpl& m_impl;
    std::string m_bundleId;
};

} // namespace apns
} // namespace push
} // namespace bcm
