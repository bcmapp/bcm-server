#include "http_session.h"
#include "http_router.h"
#include "custom_http_status.h"
#include <fiber/asio_yield.h>
#include <utils/log.h>
#include <auth/authenticator.h>
#include <boost/algorithm/string.hpp>
#include <utils/account_helper.h>
#include <crypto/gzip.h>
#include <crypto/sha1.h>
#include <websocket/websocket_session.h>

namespace bcm {

using namespace boost;

static constexpr size_t kMaxBodySize = 64 * 1024 * 1024; //64MB

HttpSession::HttpSession(std::shared_ptr<HttpService> owner,ip::tcp::socket socket)
    : m_service(std::move(owner))
    , m_stream(std::make_shared<WebsocketStrem>(std::move(socket), m_service->getSslContext()))
{
    LOGD << "create a session";
}

HttpSession::~HttpSession()
{
    LOGD << "destroy a session";
}

void HttpSession::run()
{
    auto& stream = m_stream->next_layer();
    system::error_code ec;
    beast::flat_buffer buffer;
    
    asio::deadline_timer timer(m_stream->next_layer().get_io_context());
    auto post_timer = [&]() {
        system::error_code tec;
        // async wait for 3 minutes
        timer.expires_from_now(boost::posix_time::seconds(180), tec);
        if (tec) {
            ec = http::error::end_of_stream;
            LOGE << "set timer expires failed: " << tec.message();
            return false;
        }
        timer.async_wait([self = shared_from_this()](const system::error_code &ec) {
            if (ec == asio::error::operation_aborted) {
                return;
            }

            LOGW << "cancel underlying socket, because not any io for a long time";
            system::error_code dummy{};
            self->m_stream->next_layer().next_layer().cancel(dummy);
        });
        return true;
    };

    auto cancel_timer = [&]() {
        system::error_code tec;
        timer.cancel(tec);
        if (tec) {
            ec = http::error::end_of_stream;
            LOGE << "cancel timer failed: " << tec.message();
            return false;
        }
        return true;
    };
    
    if (!post_timer()) {
        LOGE << "post_timer faild";
        system::error_code dummy{};
        m_stream->next_layer().next_layer().cancel(dummy);
        return;
    }
    
    stream.async_handshake(ssl::stream_base::server, fibers::asio::yield[ec]);
    if (ec) {
        LOGE << "handshake faild, "  << "error message: " << ec.message();
        cancel_timer();
        return;
    }
    
    if (!cancel_timer()) {
        LOGE << "cancel_timer faild";

        stream.async_shutdown(fibers::asio::yield[ec]);
        if (ec) {
            LOGE << "shutdown failed, " << ", message: " << ec.message();
        }
        return;
    }
    
    for (;;) {
        http::request_parser<http::string_body> parser;
        parser.body_limit(kMaxBodySize);

        if (!post_timer()) {
            break;
        }
        http::async_read_header(stream, buffer, parser, fibers::asio::yield[ec]);
        if (ec) {
            if (ec != http::error::end_of_stream) {
                LOGE << "read header failed, " << ", message: " << ec.message();
            }
            break;
        }

        auto& request = parser.get();
        //check upgrade
        if (websocket::is_upgrade(request)) {
            // websocket session use another timer
            if (!cancel_timer()) {
                break;
            }
            upgrade(request);
            break;
        }

        HttpContext httpContext;

        //init response
        httpContext.response.version(request.version());
        httpContext.response.result(http::status::unknown);
        httpContext.response.keep_alive(request.keep_alive());

        boost::optional<HttpRoute&> route = onHeader(request, httpContext);
        if (!route) {
            if (!sendResponse(httpContext.response)) {
                break;
            }
            continue;
        }

        //read body
        bool bExceed = false;
        while (!parser.is_done() && !ec) {
            // refresh timer to support reading big body
            if (!post_timer()) {
                break;
            }
            http::async_read_some(stream, buffer, parser, fibers::asio::yield[ec]);

            if (request.body().size() > kMaxBodySize) {
                bExceed = true;
                LOGW << "body is too large: " << request.body().size()
                     << ", content length: " << request[http::field::content_length];
                break;
            }
        }
        if (ec) {
            if (ec != http::error::end_of_stream) {
                LOGE << "read body failed: " << ec.message();
            }
            break;
        }

        if (!cancel_timer()) {
            break;
        }

        if (bExceed) {
            httpContext.response.result(http::status::payload_too_large);
            httpContext.statics.setStatus(http::status::payload_too_large);
            httpContext.statics.setMessage("max body size is " + std::to_string(kMaxBodySize));
            if (!sendResponse(httpContext.response)) {
                break;
            }
            continue;
        }

        if (!onBody(request, httpContext)) {
            if (!sendResponse(httpContext.response)) {
                break;
            }
            continue;
        }

        httpContext.request = parser.release();
        route->invokeHandler(httpContext);
        if (!sendResponse(httpContext.response)) {
            break;
        }
    }

    if (ec == http::error::end_of_stream) {
        stream.async_shutdown(fibers::asio::yield[ec]);
        if (ec) {
            LOGE << "shutdown failed" << ", message: " << ec.message();
        }
    }
}

bool HttpSession::sendResponse(http::response<http::string_body>& response)
{
    system::error_code ec;

    if (!response.body().empty()) {
        response.prepare_payload();
    }

    auto bKeep = !response.need_eof();
    if ((http::to_status_class(response.result()) == http::status_class::client_error) && response.keep_alive()) {
        bKeep = true;
    }
    response.keep_alive(bKeep);

    // When no Content-Length is received, the client keeps reading until the server closes the connection.
    if (bKeep && !response.has_content_length()) {
        response.content_length(0);
    }

    http::async_write(m_stream->next_layer(), response, fibers::asio::yield[ec]);
    if (ec) {
        LOGE << "write failed: " << ec.message();
        return false;
    }

    if (!bKeep) {
        m_stream->next_layer().async_shutdown(fibers::asio::yield[ec]);
        if (ec != asio::error::eof) {
            LOGW << "shutdown failed: " << ec.message();
        }
        return false;
    }

    return true;
}

void HttpSession::upgrade(http::request<http::string_body>& header)
{
    auto upgradeResponse = [&](http::status status, const std::string& deviceInfo = "") {
        http::response<http::string_body> response;
        response.version(header.version());
        response.keep_alive(false);
        response.result(status);
        if (!deviceInfo.empty()) {
            response.set("X-Online-Device", deviceInfo);
        }
        sendResponse(response);
    };

    system::error_code ec;
    std::string path;
    std::map<std::string, std::string> queryParams;

    HttpRouter::parseUri(header.target().to_string(), path, queryParams);

    auto upgrader = m_service->getUpgrader(path);

    if (upgrader == nullptr) {
        LOGD << "cannot find upgrader for " << path;
        return upgradeResponse(http::status::not_found);
    }

    WebsocketService::AuthRequest authInfo;
    authInfo.uid = boost::algorithm::replace_all_copy(queryParams["login"], " ", "+");
    authInfo.token = boost::algorithm::replace_all_copy(queryParams["password"], " ", "+");
    authInfo.clientVersion = header["X-Client-Version"].to_string();
    authInfo.requestId = queryParams["requestId"];
    auto authResult = upgrader->auth(authInfo);
    const std::string uid = authInfo.uid.empty() ? authInfo.requestId : authInfo.uid;

    switch (authResult.authCode) {
        case Authenticator::AUTHRESULT_SUCESS:
            break;
        case Authenticator::AUTHRESULT_ACCOUNT_DELETED:
        case Authenticator::AUTHRESULT_REQUESTID_NOT_FOUND:
            return upgradeResponse(http::status::gone);
        case Authenticator::AUTHRESULT_TOKEN_VERIFY_FAILED:
            return upgradeResponse(http::status::forbidden, authResult.deviceName);
        case Authenticator::AUTHRESULT_UNKNOWN_ERROR:
            return upgradeResponse(http::status::internal_server_error);
        case Authenticator::AUTHRESULT_ACCOUNT_NOT_FOUND:
        case Authenticator::AUTHRESULT_DEVICE_NOT_FOUND:
        case Authenticator::AUTHRESULT_DEVICE_ABNORMAL:
        case Authenticator::AUTHRESULT_AUTH_HEADER_ERROR:
        case Authenticator::AUTHRESULT_DEVICE_NOT_ALLOWED:
        default:
            return upgradeResponse(http::status::forbidden);
    }

    m_stream->control_callback([](websocket::frame_type type, boost::string_view payload) {
        boost::ignore_unused(type, payload);
        //TODO: handle control frame?

    });

    m_stream->async_accept(header, fibers::asio::yield[ec]);

    if (ec) {
        LOGE << "accept websocket failed: " << ec.message();
        return;
    }

    FiberPool::post(m_stream->next_layer().get_io_context(), &WebsocketSession::run,
                    std::make_shared<WebsocketSession>(upgrader, m_stream, authResult.authEntity, upgrader->getAuthType()));
}

boost::optional<HttpRoute&> HttpSession::onHeader(http::request<http::string_body>& header, HttpContext& context)
{
    auto& statics = context.statics;
    statics.setMethod(header.method());
    statics.setTarget(header.target().to_string());

    std::shared_ptr<IValidator> validator = m_service->getValidator();
    if (validator) {
        IValidator::ValidateInfo info;
        info.request = &header;
        try {
            auto endpoint = m_stream->next_layer().next_layer().remote_endpoint();
            info.origin = SHA1::digest(endpoint.address().to_string());
        } catch (std::exception& e) {
            LOGE << "remote_endpoint option exception:" << e.what() << std::endl;
            context.response.result(400);
            std::string reason= bcm::obsoleteReason(400);
            context.response.reason(reason);
            statics.setReason(reason);
            return boost::none;
        }

        uint32_t status;
        if (!validator->validate(info, status)) {
            context.response.result(status);
            std::string reason= bcm::obsoleteReason(status);
            context.response.reason(reason);
            statics.setReason(reason);
            return boost::none;
        }
    }

    //context.request is not available here
    auto result = m_service->getRouter().match(header, context.queryParams, context.pathParams);
    if (result.matchStatus == HttpRouter::FILTERED) {
        statics.onStart();

        context.response.result(result.filter.status);
        context.response.reason(result.filter.reason);

        statics.setReason(result.filter.reason);
        return boost::none;
    } else if (result.matchStatus == HttpRouter::MISMATCHED) {
        statics.onStart();

        context.response.result(http::status::not_found);
        statics.setStatus(http::status::not_found);
        return boost::none;
    }

    const auto authType = result.matchedRoute->getAuthType();
    if (authType != Authenticator::AUTHTYPE_NO_AUTH) {
        Account account;
        auto authHeader = AuthorizationHeader::parse(header[http::field::authorization].to_string());
        if (authHeader) {
            statics.setUid(authHeader->uid());
            statics.onStart();
            auto client = AccountHelper::parseClientVersion(header["X-Client-Version"].to_string());
            auto res = m_service->getAuthenticator().auth(authHeader.get(), client, account, authType);

            if (res == Authenticator::AUTHRESULT_SUCESS) {
                context.authResult = account;
            } else if (res == Authenticator::AUTHRESULT_UNKNOWN_ERROR) {
                context.response.result(http::status::internal_server_error);
                statics.setStatus(http::status::internal_server_error);
                statics.setMessage("unknown auth error");
                return boost::none;
            } else if (res == Authenticator::AUTHRESULT_DEVICE_NOT_ALLOWED) {
                context.response.result(http::status::forbidden);
                statics.setStatus(http::status::forbidden);
                statics.setMessage("device not allowed");
                return boost::none;
            } else {
                context.response.result(http::status::unauthorized);
                statics.setStatus(http::status::unauthorized);
                statics.setMessage("auth failed");
                return boost::none;
            }
        } else {
            statics.onStart();
            context.response.result(http::status::unauthorized);
            statics.setStatus(http::status::unauthorized);
            statics.setMessage("need auth header");
            return boost::none;
        }
    } else {
        statics.onStart();
    }

    return *result.matchedRoute;
}

bool HttpSession::onBody(http::request<http::string_body>& request, HttpContext& context)
{
    auto& statics = context.statics;

    if (request.body().empty()) {
        return true;
    }

    if (request.find(http::field::content_encoding) == request.end()) {
        return true;
    }

    if (!boost::iequals(request[http::field::content_encoding].to_string(), "gzip")) {
        context.response.result(http::status::bad_request);

        statics.setStatus(http::status::bad_request);
        statics.setMessage("compress algorithm "
                           + request[http::field::content_encoding].to_string()
                           + " is not support");
        return false;
    }

    try {
        auto decompressed = Gzip::decompress(request.body());
        if (request.find(http::field::content_length) != request.end()) {
            request.set(http::field::content_length, decompressed.size());
        }
        request.erase(http::field::content_encoding);
        request.body() = decompressed;

    } catch (std::exception& e) {
        context.response.result(http::status::bad_request);
        
        statics.setStatus(http::status::bad_request);
        statics.setMessage("gzip decompress failed!");
        return false;
    }

    return true;
}

}
