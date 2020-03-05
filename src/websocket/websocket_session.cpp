#include <utility>

#include "websocket_session.h"
#include "../limiters/limiter_executor.h"
#include "../http/custom_http_status.h"
#include <fiber/asio_yield.h>
#include <utils/log.h>
#include <boost/algorithm/string.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <memory>
#include <dispatcher/dispatch_manager.h>
#include <crypto/random.h>
#include <crypto/sha1.h>
#include <http/http_router.h>
#include <metrics_client.h>
#include <string>

namespace bcm {

using namespace boost;
using namespace metrics;

static int kKeepaliveInterval = 60; // seconds

WebsocketSession::WebsocketSession(std::shared_ptr<WebsocketService> service, 
                                   std::shared_ptr<WebsocketStrem> stream,
                                   boost::any authenticated,
                                   WebsocketService::AuthType authType)
    : m_service(std::move(service))
    , m_stream(std::move(stream))
    , m_authenticated(std::move(authenticated))
    , m_authType(authType)
{
    std::string uid;
    if (m_authType == WebsocketService::TOKEN_AUTH) {
        uid = boost::any_cast<Account>(&m_authenticated)->uid();
    } else {
        uid = *(boost::any_cast<std::string>(&m_authenticated));
    }
    LOGD << "create a websocket session for: " << uid;
    m_dummyResponse.set_status(static_cast<uint32_t>(http::status::connection_closed_without_response));
    m_dummyResponse.set_message(http::obsolete_reason(http::status::connection_closed_without_response).to_string());

    MetricsClient::Instance()->directOutput("o_active", uid);
}

WebsocketSession::~WebsocketSession()
{
    std::string uid;
    if (m_authType == WebsocketService::TOKEN_AUTH) {
        uid = boost::any_cast<Account>(&m_authenticated)->uid();
    } else {
        uid = *(boost::any_cast<std::string>(&m_authenticated));
    }
    LOGD << "destroy a websocket session for: " << uid;
}

void WebsocketSession::run()
{
    std::string uid;
    uint32_t deviceId;
    if (m_authType == WebsocketService::TOKEN_AUTH) {
        auto* account = boost::any_cast<Account>(&m_authenticated);
        uid =account->uid();
        deviceId = account->authdeviceid();
    } else {
        uid = *(boost::any_cast<std::string>(&m_authenticated));
        deviceId = kDeviceRequestLoginId;
    }
    LOGD << "establish a websocket session for: " << uid << ",deviceId:" << deviceId;
    DispatchAddress address(uid, deviceId);
    auto channelId = m_service->getDispatchMananger()->subscribe(address, shared_from_this());
    m_running = true;

    FiberPool::post(m_stream->next_layer().get_io_context(), &WebsocketSession::runWrite, shared_from_this());

    system::error_code ec;
    beast::multi_buffer readBuffer;
    asio::deadline_timer timer(m_stream->next_layer().get_io_context());

    for (;;) {
        // wait for triple of keppalive interval
        timer.expires_from_now(boost::posix_time::seconds(kKeepaliveInterval * 3), ec);
        if (ec) {
            LOGE << "set timer expires failed: " << ec.message();
            break;
        }
        timer.async_wait([self = shared_from_this()](const system::error_code &ec) {
            if (ec == asio::error::operation_aborted) {
                return;
            }
            LOGW << "cancel underlying socket, because not any io for a long time";
            system::error_code dummy{};
            self->m_stream->next_layer().next_layer().cancel(dummy);
        });

        m_stream->async_read(readBuffer, fibers::asio::yield[ec]);
        if (ec) {
            LOGE << "read failed: " << ec.message();
            break;
        }

        std::string payload = beast::buffers_to_string(readBuffer.data());
        readBuffer.consume(payload.size());

        if (m_stream->got_text()) {
            LOGW << "get a text message: " << payload;
            continue;
        }

        fibers::fiber(fibers::launch::dispatch, &WebsocketSession::dispatchMessage,
                      shared_from_this(), payload).detach();
    }

    m_service->getDispatchMananger()->unsubscribe(address, channelId);

    if (m_running) {
        m_running = false;
        m_stream->async_close(websocket::close_reason(websocket::close_code::bad_payload, "Forbidden"),
                              fibers::asio::yield[ec]);
    }
    clearPromises();

    // wakeup write loop to exit
    std::unique_lock<fibers::mutex> lk(m_writeQueueMtx);
    m_writeQueueCond.notify_one();
}

void WebsocketSession::write(const std::string& payload)
{
    std::unique_lock<fibers::mutex> lk(m_writeQueueMtx);
    m_writeQueue.push_back(payload);
    if (m_writeQueue.size() == 1) {
        m_writeQueueCond.notify_one();
    }
}

void WebsocketSession::runWrite()
{
    std::list<std::string> tmpQueue;

    while (m_running) {
        {
            std::unique_lock<fibers::mutex> lk(m_writeQueueMtx);
            if (m_writeQueue.empty()) {
                m_writeQueueCond.wait(lk);
            }

            tmpQueue.clear();
            tmpQueue.swap(m_writeQueue);
        }

        if (!m_running) {
            if (!tmpQueue.empty()) {
                LOGW << "exit write loop with remain data: " << tmpQueue.size();
            }
            break;
        }

        for (auto& payload : tmpQueue) {
            system::error_code ec;

            std::unique_lock<fibers::mutex> lk(m_writeMtx);
            m_stream->binary(true);
            m_stream->async_write(asio::buffer(payload), fibers::asio::yield[ec]);
            if (ec) {
                LOGE << "send request failed: " << ec.message();
            }
        }
    }

}

void WebsocketSession::disconnect()
{
    FiberPool::post(m_stream->next_layer().get_io_context(), [self = shared_from_this()] {
        self->m_running = false;

        std::unique_lock<fibers::mutex> lk(self->m_writeMtx);
        LOGI << "cancel underlying socket directly, because disconnection is required";
        system::error_code dummy{};
        self->m_stream->next_layer().next_layer().cancel(dummy);
    });
}

void WebsocketSession::dispatchMessage(const std::string& payload)
{
    std::string res;
    WebsocketMessage message;

    if (!message.ParseFromString(payload)) {
        LOGE << "parse websocket message failed!";
        return;
    }

    switch (message.type()) {
        case WebsocketMessage::REQUEST: {
            WebsocketResponseMessage* response = message.mutable_response();
            handleRequestMessage(message.request(), *response);
            message.clear_request();
            message.set_type(WebsocketMessage::RESPONSE);
            write(message.SerializeAsString());
            break;
        }
        case WebsocketMessage::RESPONSE: {
            handleResponseMessage(message.response());
            break;
        }
        default: {
            LOGE << "unsupport message type" << message.type();
            break;
        }
    }
}

void WebsocketSession::handleRequestMessage(const WebsocketRequestMessage& websocketReq,
                                            WebsocketResponseMessage& websocketRes)
{
    HttpContext context;
    auto& httpReq = context.request;
    auto& httpRes = context.response;
    auto& statics = context.statics;
    do {
        http::verb method = http::string_to_verb(boost::algorithm::to_upper_copy(websocketReq.verb()));

        if (method == http::verb::unknown || websocketReq.path().empty()) {
            httpRes.result(http::status::bad_request);
            break;
        }

        std::string uid;
        if (m_authType == WebsocketService::TOKEN_AUTH) {
            uid = boost::any_cast<Account>(&m_authenticated)->uid();
        } else {
            uid = *(boost::any_cast<std::string>(&m_authenticated));
        }

        statics.setPrefix("websocket");
        statics.setMethod(method);
        statics.setTarget(websocketReq.path());
        statics.setUid(uid);
        statics.onStart();

        // init context
        httpReq.method(method);
        httpReq.target(websocketReq.path());
        for (const auto& header: websocketReq.headers()) {

            std::vector<std::string> parts;
            boost::split(parts,  header, boost::is_any_of(":"));

            if (parts.size() != 2) {
                continue;
            }

            boost::trim(parts[0]);
            boost::trim(parts[1]);
            if (!parts[0].empty() && !parts[1].empty()) {
                httpReq.set(parts[0], parts[1]);
                LOGT << "add header: " << parts[0] << ":" << parts[1];
            }
        }
        httpReq.body() = websocketReq.body();
        httpRes.result(http::status::unknown);

        std::shared_ptr<IValidator> validator = m_service->getValidator();
        if (validator) {
            IValidator::ValidateInfo info;
            info.request = &httpReq;
            try {
                auto endpoint = m_stream->next_layer().next_layer().remote_endpoint();
                info.origin = SHA1::digest(endpoint.address().to_string());
            } catch (std::exception& e) {
                httpRes.result(http::status::bad_request);
                LOGE << "remote_endpoint option exception:" << e.what() << std::endl;
                break;
            }
            
            uint32_t status;
            if (!validator->validate(info, status)) {
                httpRes.result(status);
                std::string reason= bcm::obsoleteReason(status);
                httpRes.reason(reason);
                statics.setReason(reason);
                break;
            }
        }

        // handle
        auto result = m_service->getRouter().match(context);
        switch (result.matchStatus) {
            case HttpRouter::MATCHED:
                if (result.matchedRoute->getAuthType() != Authenticator::AUTHTYPE_NO_AUTH) {
                    context.authResult = getAuthenticated();
                    auto authDevice = AccountsManager::getAuthDevice(boost::any_cast<Account&>(context.authResult));
                    if (!authDevice) {
                        httpRes.result(http::status::forbidden);
                        statics.setStatus(http::status::forbidden);
                        break;
                    }
                }
                result.matchedRoute->invokeHandler(context);
                statics.setStatus(httpRes.result());
                break;
            case HttpRouter::FILTERED:
                httpRes.result(result.filter.status);
                httpRes.reason(result.filter.reason);
                statics.setReason(result.filter.reason);
                break;
            case HttpRouter::MISMATCHED:
                httpRes.result(http::status::not_found);
                statics.setStatus(http::status::not_found);
                break;
            default:
                httpRes.result(http::status::internal_server_error);
                statics.setStatus(http::status::internal_server_error);
        }
    } while (false);

    // convert response
    websocketRes.set_id(websocketReq.id());
    websocketRes.set_status(httpRes.result_int());
    websocketRes.set_message(httpRes.reason().to_string());
    for (const auto& item : httpRes) {
        websocketRes.add_headers(item.name_string().to_string() + ":" + item.value().to_string());
    }
    if (!httpRes.body().empty()) {
        websocketRes.set_body(context.response.body());
    }
}

void WebsocketSession::handleResponseMessage(const WebsocketResponseMessage& response)
{
    std::unique_lock<boost::fibers::mutex> l(m_mtx);
    auto it = m_pendingPromises.find(response.id());
    if (it == m_pendingPromises.end()) {
        LOGW << "not a pending request for id: " << response.id();
        return;
    }
    it->second->set_value(response);
    m_pendingPromises.erase(it);
}

void WebsocketSession::sendRequest(WebsocketRequestMessage& request,
                                   std::shared_ptr<fibers::promise<WebsocketResponseMessage>> promise)
{
    if (!m_running) {
        promise->set_value(m_dummyResponse);
        return;
    }

    WebsocketMessage message;
    uint64_t requestId = generateRequestId();

    {
        std::unique_lock<boost::fibers::mutex> l(m_mtx);
        m_pendingPromises[requestId] = std::move(promise);
    }

    request.set_id(requestId);
    message.set_type(WebsocketMessage::REQUEST);
    *message.mutable_request() = request;
    std::string payload = message.SerializeAsString();
    write(payload);

    checkPromisesCapacity();
}

uint64_t WebsocketSession::generateRequestId()
{
    return SecureRandom<uint64_t>::next();
}

void WebsocketSession::checkPromisesCapacity()
{
    std::unique_lock<boost::fibers::mutex> l(m_mtx);
    const static size_t kMaxPromiseSize = 100000;
    if (m_pendingPromises.size() < kMaxPromiseSize) {
        return;
    }

    LOGE << "there are too many promises";
    disconnect();
}

void WebsocketSession::clearPromises()
{
    std::unique_lock<boost::fibers::mutex> l(m_mtx);

    LOGW << "unresponse promises size: " << m_pendingPromises.size();
    for (auto& promise : m_pendingPromises) {
        LOGW << "miss response request: " << promise.first;
        promise.second->set_value(m_dummyResponse);
    }
    m_pendingPromises.clear();
}

boost::any WebsocketSession::getAuthenticated(bool bRefresh)
{
    std::unique_lock<boost::fibers::mutex> l(m_authRefreshMtx);
    if (bRefresh) {
        m_service->getAuthenticator().refreshAuthenticated(boost::any_cast<Account&>(m_authenticated));
    }
    return m_authenticated;
}

}
