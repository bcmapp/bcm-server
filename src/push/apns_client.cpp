#include <boost/fiber/future/promise.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <nghttp2/asio_http2_client.h>
#include "nlohmann/json.hpp"
#include "fiber/asio_round_robin.h"
#include "utils/log.h"
#include "apns_notification.h"
#include "apns_client.h"
#include "utils/sync_latch.h"
#include "utils/thread_utils.h"

namespace bcm {
namespace push {
namespace apns {

namespace asio = boost::asio;

typedef nghttp2::asio_http2::client::session http2_session;
typedef nghttp2::asio_http2::header_map http2_header_map;
typedef nghttp2::asio_http2::header_value http2_header_value;
typedef nghttp2::asio_http2::client::request http2_request;
typedef nghttp2::asio_http2::client::response http2_response;

typedef asio::ip::tcp::resolver resolver;
typedef asio::ssl::context ssl_context;
typedef asio::io_context io_context;
typedef boost::posix_time::time_duration time_duration;
typedef boost::system::error_code error_code;

static const std::string kApnsProductionHost = "api.push.apple.com";
static const std::string kApnsDevelopmentHost =
                                            "api.development.push.apple.com";
static const std::string kApnsDefaultPort = "443";
static const std::string kApnsHttpMethod = "POST";
static const std::string kApnsUri = "/3/device/";

void printHeader(const http2_response &res) {
    std::stringstream ss;
    ss << "HTTP/2 " << res.status_code() << "\n";
    for (auto &kv : res.header()) {
        ss << kv.first << ": " << kv.second.value << "\n";
    }
    LOGD << ss.str();
}

// -----------------------------------------------------------------------------
// Section: Request
// -----------------------------------------------------------------------------
class Request {
    const std::string& m_host;
    const Notification& m_notification;
    boost::fibers::promise<SendResult>& m_promise;
    const http2_request* m_req;
    SendResult m_result;
    std::string m_responseBody;
    boost::fibers::promise<error_code> m_respPromise;

public:
    Request(const std::string& host,
            const Notification& notification,
            boost::fibers::promise<SendResult>& promise)
        : m_host(host), m_notification(notification), m_promise(promise)
        , m_req(nullptr) {}

    bool executeOnSession(http2_session& session)
    {
#ifdef APNS_SUBMIT_FAILURE_TEST
#warning "apns submiting failure test is enabled. you can ignore this warning if this is your intention."
        if ( (double(rand()) / double(RAND_MAX)) < double(0.4) ) {
            LOGW << "intentional submiting failure detected";
            return false;
        }
#endif
        std::string token(m_notification.token());
        std::string payload(m_notification.payload());
        std::string topic(m_notification.topic());

        std::stringstream ss;
        ss << "https://" << m_host << kApnsUri << token;

        http2_header_map hm;
        hm.emplace("apns-expiration", http2_header_value{
            std::to_string(m_notification.expiryTime()), false});
        hm.emplace("apns-priority", http2_header_value{
            std::to_string(m_notification.priority()), false});
        hm.emplace("apns-topic", http2_header_value{std::move(topic), false});

        std::string collapseId(m_notification.collapseId());
        if (!collapseId.empty()) {
            hm.emplace("apns-collapse-id", http2_header_value{
                std::move(collapseId), false});
        }
        m_req = session.submit(m_result.ec, kApnsHttpMethod, ss.str(),
                               std::move(payload), std::move(hm));
        if (m_result.ec) {
            LOGE << "error submiting http2 request with token '"
                 << m_notification.token() << "' topic '"
                 << m_notification.topic() << "': " << m_result.ec.message();
            return false;
        }
        m_result.ec.clear();
        return true;
    }

    void waitResponse()
    {
        boost::fibers::future<error_code> future = m_respPromise.get_future();
        m_req->on_response([this](const http2_response& res) {
            LOGD << "response header received";
            printHeader(res);
            auto it = res.header().find("apns-id");
            if (it != res.header().end()) {
                m_result.apnsId = it->second.value;
            }
            m_result.statusCode = res.status_code();
            m_responseBody.reserve(128);
            res.on_data([this](const uint8_t* data, std::size_t len) {
                LOGD << "response data received (" << len << " bytes)";
                m_responseBody.append(reinterpret_cast<const char*>(data), len);
            });
        });
        m_req->on_close([this](uint32_t code) mutable {
            LOGD << "request done with error code " << code << ", status code " 
                 << m_result.statusCode << ", response " << m_responseBody;
            if (m_result.statusCode != int(boost::beast::http::status::ok) 
                    && !m_responseBody.empty()) {
                // if not succeed, parse error code and message
                parseResponseBody();
            }
            m_respPromise.set_value(error_code());
        });
        m_result.ec = future.get();
        m_promise.set_value(std::move(m_result));
    }

    void done(const error_code& ec)
    {
        SendResult result;
        result.ec = ec;
        m_promise.set_value(result);
    }

    void cancel(const error_code& ec)
    {
        m_respPromise.set_value(ec);
    }

private:
    void parseResponseBody()
    {
        nlohmann::json j;
        try {
            j = nlohmann::json::parse(m_responseBody);
        } catch (nlohmann::json::exception& e) {
            LOGE << "error parsing '" << m_responseBody << "':" << e.what();
            return;
        }
        nlohmann::json::iterator it = j.find("reason");
        if (it != j.end() && it->is_string()) {
            m_result.error = it->get<std::string>();
        }
        it = j.find("timestamp");
        if (it != j.end() && it->is_number()) {
            m_result.timestamp = it->get<int64_t>();
        }
    }
};

// -----------------------------------------------------------------------------
// Section: ClientImpl
// -----------------------------------------------------------------------------
static const boost::posix_time::seconds kDefaultConnectTimeout(60);
static const boost::posix_time::seconds kDefaultReadTimeout(60);

class ClientImpl {
    std::shared_ptr<http2_session> m_session;
    std::shared_ptr<io_context> m_ioc;
    std::thread m_thread;
    ssl_context m_sslCtx;

    time_duration m_connectTimeout;
    time_duration m_readTimeout;
    const std::string* m_host;
    std::string  m_bundleId;
    std::string  m_type;
    
    enum ConnectionState {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
    };

    ConnectionState m_connectionState;
    boost::mutex m_connectionStateMutex;
    std::set<Request*> m_pendingRequests;
    boost::fibers::mutex m_pendingRequestsMutex;
public:
    ClientImpl()
        : m_ioc(std::make_shared<io_context>())
        , m_thread(&ClientImpl::run, this)
        , m_sslCtx(asio::ssl::context::sslv23)
        , m_connectTimeout(kDefaultConnectTimeout)
        , m_readTimeout(kDefaultReadTimeout)
        , m_host(&kApnsProductionHost)
        , m_bundleId("")
        , m_type("")
        , m_connectionState(DISCONNECTED)
    {
        m_sslCtx.set_default_verify_paths();
        m_sslCtx.set_verify_mode(asio::ssl::verify_peer |
                                 asio::ssl::verify_fail_if_no_peer_cert);
        error_code ec;
        nghttp2::asio_http2::client::configure_tls_context(ec, m_sslCtx);
        asio::detail::throw_error(ec);

        SyncLatch sl(2);
        m_ioc->post([&sl]() {
            LOGD << "wait apns client thread to be started";
            sl.sync();
        });
        sl.sync();
        LOGD << "apns client is running";
    }

    ~ClientImpl()
    {
        if (!m_ioc->stopped()) {
            SyncLatch sl(2);
            m_ioc->post([this, &sl]() mutable {
                LOGD << "stop apns client thread";
                LOGD << "client state: " << connectionState();
                if (CONNECTED == connectionState()) {
                    m_session->shutdown();
                }
                m_ioc->stop();
                sl.sync();
            });
            sl.sync();
            m_thread.join();
            LOGD << "apns client is shutdown";
        } else {
            LOGW << "apns client is already shutdown";
        }
    }

    void certificateFile(const std::string& path, error_code& ec) noexcept
    {
        m_sslCtx.use_certificate_file(path, ssl_context::pem, ec);
    }

    void privateKeyFile(const std::string& path, error_code& ec) noexcept
    {
        m_sslCtx.use_rsa_private_key_file(path, ssl_context::pem, ec);
    }

    void connectTimeout(const time_duration& timeout) noexcept
    {
        m_connectTimeout = timeout;
    }

    void readTimeout(const time_duration& timeout) noexcept
    {
        m_readTimeout = timeout;
    }

    void host(const std::string* host) noexcept
    {
        m_host = host;
    }
    
    void setBundleId(const std::string& bundleId) noexcept
    {
        m_bundleId = bundleId;
    }
    
    const std::string bundleId() const noexcept
    {
        return m_bundleId;
    }
    
    void setType(const std::string& type) noexcept
    {
        m_type = type;
    }
    
    const std::string type() const noexcept
    {
        return m_type;
    }
    
    void start()
    {
        m_ioc->post([this]() {
            LOGD << "start a fiber to connect to apns server, m_bundleId: "
                    << this->bundleId() << ", type: " << this->type() ;
            boost::fibers::fiber([this] {
                error_code ec;
                this->connect(ec);
            }).detach();
        });
    }

    SendResult send(const Notification& notification) noexcept
    {
        boost::fibers::promise<SendResult> promise;
        boost::fibers::future<SendResult> future = promise.get_future();
        Request request(*m_host, notification, promise);
        m_ioc->post([this, &request]() {
            boost::fibers::fiber(&ClientImpl::doHttpRequest, this,
                                 std::ref(request)).detach();
        });
        return future.get();
    }

private:
    void run()
    {
        setCurrentThreadName("push.apns");
        try {
            fibers::asio::round_robin rr(m_ioc);
            rr.run();
        } catch (std::exception& e) {
            LOGE << "exception caught: " << e.what();
        }
    }

    void connect(error_code& ec) noexcept
    {
        boost::fibers::promise<error_code> promise;
        boost::fibers::future<error_code> future = promise.get_future();
        m_ioc->post([this, &promise]() {
            LOGD << "start a fiber to create a new http2 session, bundleId: "
                    << this->bundleId() << ", type: " << this->type();
            boost::fibers::fiber(
                &ClientImpl::newSession, this, std::ref(promise)).detach();
        });
        ec = future.get();
    }

    void newSession(boost::fibers::promise<error_code>& promise)
    {
        if ( (CONNECTING == m_connectionState)
                || (CONNECTED == m_connectionState) ) {
            promise.set_value(asio::error::already_connected);
            return;
        }
        m_connectionState = CONNECTING;

        LOGD << "connect to '" << *m_host << ":" << kApnsDefaultPort
             << "', bundleId: " << m_bundleId << ", type: " << m_type;
        m_session = std::make_shared<http2_session>(*m_ioc,
                                                    m_sslCtx, *m_host,
                                                    kApnsDefaultPort,
                                                    m_connectTimeout);
        m_session->read_timeout(m_readTimeout);
        m_session->on_connect([this, sess(m_session), &promise](
            resolver::iterator iter) {
            boost::ignore_unused(iter);
            LOGI << "'" << *m_host << ":" << kApnsDefaultPort << "', type: " << m_type << ", connected";
            sess->on_error(std::bind(&ClientImpl::handleError, this,
                                     std::placeholders::_1));
            connectionState(CONNECTED);
            promise.set_value(error_code());
        });
        m_session->on_error([this, sess(m_session), &promise](
            const error_code& ec) mutable {
            LOGE << "could not connect to '" << *m_host << ":" << kApnsDefaultPort
                 << "', bundleId: " << m_bundleId
                 << ", type: " << m_type
                 << ", message: " << ec.message();
            sess.reset();
            connectionState(DISCONNECTED);
            promise.set_value(ec);
        });
    }

    void handleError(const boost::system::error_code& ec)
    {
        LOGE << "connection to '" << *m_host << ":" << kApnsDefaultPort
             << "', bundleId: " << m_bundleId
             << ", type: " << m_type
             << " is broken: " << ec.message();
        m_session.reset();
        connectionState(DISCONNECTED);
        start();
        for (auto req : m_pendingRequests) {
            req->cancel(ec);
        }
        m_pendingRequests.clear();
    }

    void doHttpRequest(Request& request)
    {
        if (connectionState() != CONNECTED) {
            error_code ec;
            connect(ec);
            if (ec) {
                LOGE << "error connecting to '" << *m_host
                     << "', bundleId: " << m_bundleId
                     << ", type: " << m_type
                     << "', message: " << ec.message();
                request.done(ec);
                return;
            } else {
                LOGI << "host '" << *m_host
                     << "', bundleId: " << m_bundleId
                     << ", type: " << m_type
                     << "' connected";
            }
        }
        if (request.executeOnSession(*m_session)) {
            m_pendingRequests.insert(&request);
            request.waitResponse();
            m_pendingRequests.erase(&request);
            return;
        }
        LOGI << "failed to execute apns request. attempt to reconnect host: '" << *m_host
             << "', bundleId: " << m_bundleId
             << ", type: " << m_type;
        m_session->shutdown();
        connectionState(DISCONNECTED);
        request.done(asio::error::try_again);
    }

    ConnectionState connectionState() const
    {
        return m_connectionState;
    }

    void connectionState(ConnectionState s)
    {
        m_connectionState = s;
    }

#ifdef APNS_TEST
public:
    void start(error_code& ec)
    {
        boost::fibers::promise<error_code> promise;
        boost::fibers::future<error_code> future = promise.get_future();
        m_ioc->post([this, &promise]() {
            boost::fibers::fiber([this, &promise] {
                error_code ec;
                this->connect(ec);
                promise.set_value(ec);
            }).detach();
        });
        ec = future.get();
    }

    void restart(error_code& ec)
    {
        boost::fibers::promise<error_code> promise;
        boost::fibers::future<error_code> future = promise.get_future();
        m_ioc->post([this, &promise]() {
            boost::fibers::fiber([this, &promise] {
                if (m_session) {
                    m_session->shutdown();
                }
                connectionState(DISCONNECTED);

                error_code ec;
                this->connect(ec);
                promise.set_value(ec);
            }).detach();
        });
        ec = future.get();
    }

    void shutdown()
    {
        boost::fibers::promise<void> promise;
        boost::fibers::future<void> future = promise.get_future();
        m_ioc->post([this, &promise]() {
            boost::fibers::fiber([this, &promise] {
                if (m_session) {
                    m_session->shutdown();
                }
                connectionState(DISCONNECTED);
                promise.set_value();
            }).detach();
        });
        future.get();
    }
#endif
};

// -----------------------------------------------------------------------------
// Section: SendResult
// -----------------------------------------------------------------------------
SendResult::SendResult(const boost::system::error_code& ec_)
    : ec(ec_), statusCode(0), timestamp(0) {}

bool SendResult::isUnregistered() const
{
    return (error.compare("Unregistered") == 0);
}

// -----------------------------------------------------------------------------
// Section: Client
// -----------------------------------------------------------------------------
Client::Client() : m_pImpl(new ClientImpl()), m_impl(*m_pImpl) {}

Client::~Client()
{
    if (m_pImpl != nullptr) {
        delete m_pImpl;
    }
}

Client& Client::bundleId(const std::string& bundleId) noexcept
{
    m_bundleId = bundleId;
    m_pImpl->setBundleId(bundleId);
    return *this;
}

Client& Client::production() noexcept
{
    m_impl.host(&kApnsProductionHost);
    return *this;
}

Client& Client::development() noexcept
{
    m_impl.host(&kApnsDevelopmentHost);
    return *this;
}

Client& Client::certificateFile(const std::string& path)
{
    error_code ec;
    m_impl.certificateFile(path, ec);
    asio::detail::throw_error(ec);
    return *this;
}

Client& Client::privateKeyFile(const std::string& path)
{
    error_code ec;
    m_impl.privateKeyFile(path, ec);
    asio::detail::throw_error(ec);
    return *this;
}

Client& Client::connectTimeout(const time_duration& timeout) noexcept
{
    m_impl.connectTimeout(timeout);
    return *this;
}

Client& Client::readTimeout(const time_duration& timeout) noexcept
{
    m_impl.readTimeout(timeout);
    return *this;
}

const std::string& Client::bundleId() const noexcept
{
    return m_bundleId;
}
void Client::setType(const std::string& type) noexcept
{
    m_impl.setType(type);
    return;
}

void Client::start() noexcept
{
    m_impl.start();
}

SendResult Client::send(const Notification& notification) noexcept
{
    return m_impl.send(notification);
}

#ifdef APNS_TEST
void Client::start(error_code& ec) noexcept
{
    m_impl.start(ec);
}

void Client::restart(error_code& ec) noexcept
{
    m_impl.restart(ec);
}

void Client::shutdown() noexcept
{
    m_impl.shutdown();
}
#endif

} // namespace apns
} // namespace push
} // namespace bcm