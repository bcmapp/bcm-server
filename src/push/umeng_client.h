#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace bcm {
namespace push {
namespace umeng {

class Notification;

enum class AppVer {
    V1 = 1,
    V2
};

// -----------------------------------------------------------------------------
// Section: SendResult
// -----------------------------------------------------------------------------
struct SendResult {
    boost::system::error_code ec;
    boost::beast::http::status statusCode;
    std::string ret;
    std::string msgId;
    std::string taskId;
    std::string errorCode;
    std::string errorMsg;
};

// -----------------------------------------------------------------------------
// Section: Client
// -----------------------------------------------------------------------------
class Client : private boost::noncopyable {
public:
    Client() {}

    Client& appKey(const std::string& key);
    Client& appMasterSecret(const std::string& secret);
    Client& appKeyV2(const std::string& key);
    Client& appMasterSecretV2(const std::string& secret);
    const std::string& appKey() const;
    const std::string& appMasterSecret() const;
    const std::string& appKeyV2() const;
    const std::string& appMasterSecretV2()const;
    SendResult send(boost::asio::io_context& ioc, const Notification& n, 
                    AppVer ver = AppVer::V1);

private:
    static std::string sign(const std::string& postData, 
                            const std::string& appMasterSecret);

private:
    std::string m_appKey;
    std::string m_appMasterSecret;
    std::string m_appKeyV2;
    std::string m_appMasterSecretV2;
};

} // namespace umeng
} // namespace push
} // namespace bcm
