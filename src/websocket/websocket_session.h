#pragma once

#include "websocket_service.h"
#include <proto/websocket/websocket_protocol.pb.h>
#include <boost/fiber/future.hpp>

namespace bcm {

constexpr uint32_t kDeviceRequestLoginId = 65536;
namespace fibers = boost::fibers;
using WebsocketStrem = websocket::stream<ssl::stream<ip::tcp::socket>>;

class WebsocketSession : public std::enable_shared_from_this<WebsocketSession> {
public:
    WebsocketSession(std::shared_ptr<WebsocketService> service,
                     std::shared_ptr<WebsocketStrem> stream,
                     boost::any authenticated,
                     WebsocketService::AuthType authType = WebsocketService::TOKEN_AUTH);
    ~WebsocketSession();

    void run();
    void disconnect();
    void sendRequest(WebsocketRequestMessage& request,
                     std::shared_ptr<fibers::promise<WebsocketResponseMessage>> promise);
    boost::any getAuthenticated(bool bRefresh = true);
    WebsocketService::AuthType getAuthType() {return m_authType;}

private:
    void write(const std::string& payload);
    void runWrite();
    void dispatchMessage(const std::string& payload);
    void handleRequestMessage(const WebsocketRequestMessage& websocketReq, WebsocketResponseMessage& websocketRes);
    void handleResponseMessage(const WebsocketResponseMessage& response);
    uint64_t generateRequestId();
    void checkPromisesCapacity();
    void clearPromises();

private:
    std::atomic_bool m_running{false};
    std::shared_ptr<WebsocketService> m_service;
    std::shared_ptr<WebsocketStrem> m_stream;
    boost::any m_authenticated;
    WebsocketService::AuthType m_authType;
    WebsocketResponseMessage m_dummyResponse;
    fibers::mutex m_mtx;
    std::map<uint64_t, std::shared_ptr<fibers::promise<WebsocketResponseMessage>>> m_pendingPromises;
    fibers::mutex m_writeMtx;
    fibers::mutex m_writeQueueMtx;
    fibers::mutex m_authRefreshMtx;
    fibers::condition_variable m_writeQueueCond;
    std::list<std::string> m_writeQueue;
};

}


