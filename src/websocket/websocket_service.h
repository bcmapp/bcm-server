#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/fiber/all.hpp>
#include <boost/beast.hpp>

#include <http/http_router.h>
#include <http/http_validator.h>
#include <fiber/fiber_pool.h>
#include "auth/authenticator.h"
#include "dispatcher/dispatch_manager.h"

namespace bcm {

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace ssl = boost::asio::ssl;
namespace http = boost::beast::http;
namespace websocket = boost::beast::websocket;
namespace fibers = boost::fibers;

class WebsocketService : public std::enable_shared_from_this<WebsocketService> {

public:
    enum AuthType {
        TOKEN_AUTH = 0,
        REQUESTID_AUTH = 1
    };

    struct AuthRequest {
        std::string uid;
        std::string token;
        std::string clientVersion;
        std::string requestId;
    };

    struct AuthResult {
        Authenticator::AuthResult authCode;
        boost::any authEntity;
        std::string deviceName;
    };

public:
    WebsocketService(std::string path,
                     std::shared_ptr<ssl::context> sslCtx,
                     std::shared_ptr<HttpRouter> router,
                     std::shared_ptr<Authenticator> authenticator,
                     std::shared_ptr<DispatchManager> dspatchManager,
                     size_t concurrency,
                     const std::shared_ptr<IValidator>& validator,
                     AuthType authType = TOKEN_AUTH);

    void run(std::string ip, uint16_t port);
    void stop();

    WebsocketService::AuthResult auth(const AuthRequest& authInfo);
    bool match(const std::string& path);
    HttpRouter& getRouter() { return *m_router; }
    ssl::context& getSslContext() { return *m_sslCtx; }
    const Authenticator& getAuthenticator() { return *m_authenticator; }
    AuthType getAuthType() const { return m_authType; }
    std::shared_ptr<IValidator> getValidator() { return m_validator; }
    std::shared_ptr<DispatchManager> getDispatchMananger() { return m_dspatchManager; }

private:
    void loop(std::shared_ptr<asio::io_context> ioc, std::string& ip, uint16_t port);

private:
    std::string m_path;
    //bool m_running{false};
    //std::thread m_thread;
    std::shared_ptr<ssl::context> m_sslCtx;
    std::shared_ptr<HttpRouter> m_router;
    std::shared_ptr<Authenticator> m_authenticator;
    std::shared_ptr<DispatchManager> m_dspatchManager;
    FiberPool m_execPool;
    std::shared_ptr<IValidator> m_validator;
    AuthType m_authType;
};

}
