#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <fiber/fiber_pool.h>
#include <websocket/websocket_service.h>
#include "http_router.h"
#include "auth/authenticator.h"
#include "http_validator.h"

namespace bcm {

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace ssl = boost::asio::ssl;

class HttpService : public std::enable_shared_from_this<HttpService> {
public:
    HttpService(
            std::shared_ptr<ssl::context> sslCtx
            , std::shared_ptr<HttpRouter> router
            , std::shared_ptr<Authenticator> authenticator
            , size_t concurrency
            , std::shared_ptr<IValidator> validator);
    ~HttpService() = default;

    void run(std::string ip, uint16_t port);
    void wait();
    void stop();

    void enableUpgrade(std::shared_ptr<WebsocketService> upgrader) { m_upgraders.push_back(std::move(upgrader)); }

    HttpRouter& getRouter() { return *m_router; }
    ssl::context& getSslContext() { return *m_sslCtx; }
    const Authenticator& getAuthenticator() { return *m_authenticator; }

    std::shared_ptr<IValidator> getValidator() { return m_validator; }

    std::shared_ptr<WebsocketService> getUpgrader(const std::string& path);

private:
    void loop(std::shared_ptr<asio::io_context> ioc, std::string ip, uint16_t port);

private:
    bool m_running{false};
    std::thread m_thread;
    std::shared_ptr<ssl::context> m_sslCtx;
    std::shared_ptr<HttpRouter> m_router;
    std::vector<std::shared_ptr<WebsocketService>> m_upgraders;
    std::shared_ptr<Authenticator> m_authenticator;
    FiberPool m_execPool;
    std::shared_ptr<IValidator> m_validator;
};

}



