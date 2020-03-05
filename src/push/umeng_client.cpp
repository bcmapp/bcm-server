#include <openssl/md5.h>
#include "fiber/asio_yield.h"
#include "utils/log.h"
#include "umeng_notification.h"
#include "umeng_client.h"
#include "crypto/hex_encoder.h"

namespace bcm {
namespace push {
namespace umeng {

namespace http = boost::beast::http;

using tcp = boost::asio::ip::tcp;
using yield_t = boost::fibers::asio::yield_t;
using error_code = boost::system::error_code;

static const int kUmengHttpVersion = 11;
static const std::string kUmengHost = "msg.umeng.com";
static const std::string kUmengPort = "80";
static const std::string kUmengUri  = "/api/send";
static const std::string kUmengUserAgent = "Mozilla/5.0";
static const std::string kUmengContentType = "application/json";

typedef http::request<http::string_body> http_request_t;
typedef http::response<http::string_body> http_response_t;

std::string md5(const std::string& data)
{
    MD5_CTX ctx;
    uint8_t digest[MD5_DIGEST_LENGTH];
    MD5_Init(&ctx);
    MD5_Update(&ctx, data.c_str(), data.size());
    MD5_Final(digest, &ctx);
    return HexEncoder::encode(std::string(reinterpret_cast<char*>(digest), sizeof(digest)));
}

// -----------------------------------------------------------------------------
// Section: Request
// -----------------------------------------------------------------------------
class Request {
    tcp::resolver m_resolver;
    tcp::socket m_socket;
    boost::beast::flat_buffer m_buffer;
    http_request_t m_req;
    http_response_t m_res;
    nlohmann::json m_body;

public:
    explicit Request(boost::asio::io_context& ioc)
        : m_resolver(ioc), m_socket(ioc)
    {
        m_req.version(kUmengHttpVersion);
        m_req.method(http::verb::post);
        m_req.set(http::field::host, kUmengHost);
        m_req.set(http::field::user_agent, kUmengUserAgent);
        m_req.set(http::field::content_type, kUmengContentType);
    }

    Request& uri(std::string uri)
    {
        m_req.target(std::move(uri));
        return *this;
    }

    Request& postData(std::string data)
    {
        m_req.set(http::field::content_length, data.size());
        m_req.body() = std::move(data);
        return *this;
    }

    void execute(yield_t& yield, error_code& ec)
    {
        establishConnection(yield, ec);
        if (!ec) {
            doExecute(yield, ec);

            // Ignore error issued by shutdown
            error_code ignored;
            m_socket.shutdown(tcp::socket::shutdown_both, ignored);
        }
    }

    void getResult(SendResult& result)
    {
        result.statusCode = m_res.result();
        nlohmann::json::const_iterator it = m_body.find("ret");
        if (it != m_body.end()) {
            it->get_to(result.ret);
        }
        nlohmann::json::const_iterator itData = m_body.find("data");
        if (itData == m_body.end()) {
            return;
        }
        const nlohmann::json& jData = *itData;
        it = jData.find("msg_id");
        if (it != jData.end()) {
            it->get_to(result.msgId);
        }
        it = jData.find("task_id");
        if (it != jData.end()) {
            it->get_to(result.taskId);
        }
        it = jData.find("error_code");
        if (it != jData.end()) {
            it->get_to(result.errorCode);
        }
        it = jData.find("error_msg");
        if (it != jData.end()) {
            it->get_to(result.errorMsg);
        }
    }

private:
    void establishConnection(yield_t& yield, error_code& ec)
    {
        tcp::resolver::results_type results = m_resolver.async_resolve(
            kUmengHost, kUmengPort, yield[ec]);
        if (ec) {
            LOGE << "resolve '" << kUmengHost << ":" << kUmengPort
                 << "' error: " << ec.message();
            return;
        }
        boost::asio::async_connect(m_socket, results.begin(), results.end(),
                                   yield[ec]);
        if (ec) {
            LOGE << "connect '" << kUmengHost << ":" << kUmengPort
                 << "' error: " << ec.message();
        }
    }

    void doExecute(yield_t& yield, error_code& ec)
    {
        http::async_write(m_socket, m_req, yield[ec]);
        if (ec) {
            LOGE << "send request error: " << ec.message();
            return;
        }
        http::async_read(m_socket, m_buffer, m_res, yield[ec]);
        if (ec) {
            LOGE << "receive response error: " << ec.message();
            return;
        }
        doParseResponseBody();
    }

    void doParseResponseBody()
    {
        LOGD << "parse umeng response " << m_res.body();
        try {
            m_body = nlohmann::json::parse(m_res.body());
        } catch (nlohmann::json::exception& e) {
            LOGE << "parse '" << m_res.body() << "' error: " << e.what();
        }
    }
};

// -----------------------------------------------------------------------------
// Section: Client
// -----------------------------------------------------------------------------
Client& Client::appKey(const std::string& key)
{
    m_appKey = key;
    return *this;
}

Client& Client::appMasterSecret(const std::string& secret)
{
    m_appMasterSecret = secret;
    return *this;
}

Client& Client::appKeyV2(const std::string& key)
{
    m_appKeyV2 = key;
    return *this;
}

Client& Client::appMasterSecretV2(const std::string& secret)
{
    m_appMasterSecretV2 = secret;
    return *this;
}

const std::string& Client::appKey() const
{
    return m_appKey;
}

const std::string& Client::appMasterSecret() const
{
    return m_appMasterSecret;
}

const std::string& Client::appKeyV2() const
{
    return m_appKeyV2;
}

const std::string& Client::appMasterSecretV2()const
{
    return m_appMasterSecretV2;
}

SendResult Client::send(boost::asio::io_context& ioc, 
                        const Notification& n, AppVer ver)
{
    SendResult result;
    std::string postData = n.serialize();
    std::string sign = (ver == AppVer::V1 ) ? 
        Client::sign(postData, m_appMasterSecret) : 
        Client::sign(postData, m_appMasterSecretV2);
    Request request(ioc);
    request.uri(kUmengUri + "?sign=" + sign).postData(postData)
        .execute(boost::fibers::asio::yield, result.ec);
    if (result.ec) {
        LOGE << "error send umeng notification: " << result.ec.message();
    } else {
        request.getResult(result);
    }
    return result;
}

std::string Client::sign(const std::string& postData,
                         const std::string& appMasterSecret)
{
    static const std::string kUmengMethod = "POST";
    static const std::string kUmengScheme = "http://";
    std::string str;
    str.reserve(kUmengMethod.size() + kUmengScheme.size() +
                kUmengHost.size() + kUmengUri.size() + postData.size() +
                appMasterSecret.size() + 1);
    str.append(kUmengMethod).append(kUmengScheme).append(kUmengHost)
        .append(kUmengUri).append(postData).append(appMasterSecret);
    std::string md5hex(md5(str));
    LOGD << "sign data '" << postData << "' with secret '" 
         << appMasterSecret << "': " << md5hex;
    return md5hex;
}

} // namespace umeng
} // namespace push
} // namespace bcm