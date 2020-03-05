#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>
#include "fiber/asio_yield.h"
#include "utils/log.h"
#include "fcm_notification.h"
#include "fcm_client.h"

namespace bcm {
namespace push {
namespace fcm {

using yield_t = boost::fibers::asio::yield_t;
using error_code = boost::system::error_code;
using tcp = boost::asio::ip::tcp;

namespace ssl = boost::asio::ssl;
namespace http = boost::beast::http;

static const int kFcmHttpVersion = 11;
static const std::string kFcmHost = "fcm.googleapis.com";
static const std::string kFcmPort = "443";
static const std::string kFcmUri  = "/fcm/send";
static const std::string kFcmContentType = "application/json";
static const std::string kFcmHeaderAuth = "Authorization";

typedef http::request<http::string_body> http_request_t;
typedef http::response<http::string_body> http_response_t;

// -----------------------------------------------------------------------------
// Section: Request
// -----------------------------------------------------------------------------
class Request {
    tcp::resolver m_resolver;
    ssl::stream<tcp::socket> m_stream;
    boost::beast::flat_buffer m_buffer;
    http_request_t m_req;
    http_response_t m_res;
    bool m_topicNotify = false;
    nlohmann::json m_result;

public:
    Request(boost::asio::io_context& ioc, ssl::context& ctx, bool topicNotify)
        : m_resolver(ioc), m_stream(ioc, ctx)
    {
        m_req.version(kFcmHttpVersion);
        m_req.method(http::verb::post);
        m_req.target(kFcmUri);
        m_req.set(http::field::host, kFcmHost);
        m_req.set(http::field::content_type, kFcmContentType);
        m_topicNotify = topicNotify;
    }

    Request& apiKey(const std::string& key)
    {
        m_req.set(kFcmHeaderAuth, "key=" + key);
        return *this;
    }

    Request& postData(std::string data)
    {
        m_req.set(http::field::content_length, data.size());
        m_req.body().assign(std::move(data));
        return *this;
    }

    void execute(yield_t& yield, error_code& ec)
    {
        establishConnection(yield, ec);
        if (!ec) {
            doExecute(yield, ec);

            // Ignore error issued by shutdown
            error_code ignored;
            m_stream.async_shutdown(yield[ignored]);
        }
    }

    void getResult(SendResult& result)
    {
        result.statusCode = m_res.result();
        nlohmann::json::const_iterator it = m_result.find("registration_id");
        if (it != m_result.end()) {
            result.canonicalRegistrationId = 
                it->get_to(result.canonicalRegistrationId);
        }
        it = m_result.find("message_id");
        if (it != m_result.end() && m_result["message_id"].is_string()) {
            it->get_to(result.messageId);
        }
        it = m_result.find("error");
        if (it != m_result.end() && m_result["error"].is_string()) {
            it->get_to(result.error);
        }
    }

private:
    void establishConnection(yield_t& yield, error_code& ec)
    {
        if (!SSL_set_tlsext_host_name(m_stream.native_handle(),
                                      kFcmHost.c_str())) {
            ec.assign(static_cast<int>(::ERR_get_error()),
                      boost::asio::error::get_ssl_category());
            return;
        }
        tcp::resolver::results_type results = m_resolver.async_resolve(
            kFcmHost, kFcmPort, yield[ec]);
        if (ec) {
            LOGE << "resolve '" << kFcmHost << ":" << kFcmPort << "' error: "
                 << ec.message();
            return;
        }
        boost::asio::async_connect(m_stream.next_layer(), results.begin(),
                                   results.end(), yield[ec]);
        if (ec) {
            LOGE << "connect '" << kFcmHost << ":" << kFcmPort << "' error: "
                 << ec.message();
            return;
        }
        m_stream.async_handshake(ssl::stream_base::client, yield[ec]);
        if (ec) {
            LOGE << "handshake error: " << ec.message()
                 << ", shutdown connection";
            m_stream.async_shutdown(yield[ec]);
        }
    }

    void doExecute(yield_t& yield, error_code& ec)
    {
        http::async_write(m_stream, m_req, yield[ec]);
        if (ec) {
            LOGE << "send request error: " << ec.message();
            return;
        }
        http::async_read(m_stream, m_buffer, m_res, yield[ec]);
        if (ec) {
            LOGE << "receive response error: " << ec.message();
            return;
        }

        if (m_res.result() != http::status::ok)
        {
            LOGE << "receive http response status error: " << m_res.result();
            return;
        }

        // If no data is post, parsing response body will cause an error
        if (!m_req.body().empty()) {
            doParseResponseBody();
        }
    }

    void doParseResponseBody()
    {
        nlohmann::json body;
        LOGD << "parse fcm response " << m_res.body();
        try {
            body = nlohmann::json::parse(m_res.body());
        } catch (nlohmann::json::exception& e) {
            LOGE << "parse '" << m_res.body() << "' error: " << e.what();
            return;
        }
        if (!m_topicNotify) {
            nlohmann::json::const_iterator it = body.find("results");
            if ( (it != body.end()) && it->is_array() && !it->empty() ) {
                m_result = it->at(0);
            }
        } else {
            m_result = body;
        }
    }
};

// -----------------------------------------------------------------------------
// Section: SendResult
// -----------------------------------------------------------------------------
SendResult::SendResult() : statusCode(boost::beast::http::status::unknown) {}

bool SendResult::isSuccess() const
{
    return ( !messageId.empty() && error.empty() );
}

bool SendResult::isUnregistered() const
{
    return (error.compare("NotRegistered") == 0);
}

bool SendResult::isThrottled() const
{
    return (error.compare("DeviceMessageRateExceeded") == 0);
}

bool SendResult::isInvalidRegistrationId() const
{
    return (error.compare("InvalidRegistration") == 0);
}

bool SendResult::hasCanonicalRegistrationId() const
{
    return !canonicalRegistrationId.empty();
}

// -----------------------------------------------------------------------------
// Section: Client
// -----------------------------------------------------------------------------
Client::Client() : m_sslCtx(ssl::context::sslv23)
{
    m_sslCtx.set_default_verify_paths();
    m_sslCtx.set_verify_mode(ssl::verify_peer);
}

Client& Client::apiKey(const std::string& key)
{
    m_apiKey = key;
    return *this;
}

SendResult Client::send(boost::asio::io_context& ioc, const Notification& n, bool topicNotify)
{
    LOGD << "send fcm notification '" << n.serialize() << "'";
    SendResult result;
    Request request(ioc, m_sslCtx, topicNotify);
    request.apiKey(m_apiKey).postData(n.serialize()).execute(
        boost::fibers::asio::yield, result.ec);
    if (!result.ec) {
        request.getResult(result);
    }
    return result;
}

bool Client::checkConnectivity(boost::asio::io_context& ioc)
{
    error_code ec;
    Request request(ioc, m_sslCtx, false);
    request.apiKey(m_apiKey).postData("")
            .execute(boost::fibers::asio::yield, ec);
    return !ec;
}

} // namespace fcm
} // namespace push
} // namespace bcm
