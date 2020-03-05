#pragma once

#include <string>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>

namespace bcm {
namespace push {
namespace fcm {

class Notification;
// -----------------------------------------------------------------------------
// Section: SendResult
// -----------------------------------------------------------------------------
struct SendResult {
    boost::system::error_code ec;
    boost::beast::http::status statusCode;
    std::string canonicalRegistrationId;
    std::string messageId;
    std::string error;

    SendResult();
    SendResult(const SendResult& other) = default;
    SendResult(SendResult&& other) = default;
    SendResult& operator=(const SendResult& other) = default;
    SendResult& operator=(SendResult&& other) = default;

    bool isSuccess() const;
    bool isUnregistered() const;
    bool isThrottled() const;
    bool isInvalidRegistrationId() const;
    bool hasCanonicalRegistrationId() const;
};

// -----------------------------------------------------------------------------
// Section: Client
// -----------------------------------------------------------------------------
class Client : private boost::noncopyable {
public:
    Client();

    Client& apiKey(const std::string& key);
    SendResult send(boost::asio::io_context& ioc, const Notification& n, bool topicNotify = false);
    bool checkConnectivity(boost::asio::io_context& ioc);

private:
    boost::asio::ssl::context m_sslCtx;
    std::string m_apiKey;
};

} // namespace fcm
} // namespace push
} // namespace bcm
