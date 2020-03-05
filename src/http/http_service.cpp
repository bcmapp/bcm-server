#include "http_service.h"
#include "http_session.h"
#include <fstream>
#include <utility>
#include <boost/fiber/all.hpp>
#include <fiber/asio_round_robin.h>
#include <fiber/asio_yield.h>
#include <utils/log.h>
#include <utils/sync_latch.h>
#include <utils/thread_utils.h>

namespace bcm {

using namespace boost;

HttpService::HttpService(
        std::shared_ptr<ssl::context> sslCtx
        , std::shared_ptr<HttpRouter> router
        , std::shared_ptr<Authenticator> authenticator
        , size_t concurrency
        , std::shared_ptr<IValidator> validator)
    : m_sslCtx(std::move(sslCtx))
    , m_router(std::move(router))
    , m_authenticator(std::move(authenticator))
    , m_execPool(concurrency)
    , m_validator(std::move(validator))
{

}

void HttpService::run(std::string ip, uint16_t port)
{
    m_execPool.run("http.worker");

    SyncLatch sl(2);
    m_thread = std::thread([&] {
        m_running = true;
        setCurrentThreadName("http.service");
        auto ioc = std::make_shared<asio::io_context>();
        fibers::asio::round_robin rr(ioc);
        fibers::fiber(std::bind(&HttpService::loop, shared_from_this(), ioc, ip, port)).detach();
        sl.sync();
        rr.run();
    });
    sl.sync();
}

void HttpService::wait()
{
    m_thread.join();
}

void HttpService::stop()
{
    m_running = false;
    //m_thread.join();

    m_execPool.stop();
}

void HttpService::loop(std::shared_ptr<asio::io_context> ioc, std::string ip, uint16_t port)
{
    ip::tcp::endpoint endpoint(ip::make_address(ip), port);
    ip::tcp::acceptor acceptor(*ioc);
    system::error_code ec;

    acceptor.open(endpoint.protocol(), ec);
    if (ec) {
        LOGE << "open failed: " << ec.message();
        return;
    }

    acceptor.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec) {
        LOGE << "set_option failed: " << ec.message();
        return;
    }

    acceptor.bind(endpoint, ec);
    if (ec) {
        LOGE << "bind failed: " << ec.message();
        return;
    }

    acceptor.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) {
        LOGE << "listen failed: " << ec.message();
        return;
    }

    LOGI << "start listen to " << ip << ":" << port;

    while (m_running) {
        ec.clear();
        auto& execIoc = m_execPool.getIOContext();
        ip::tcp::socket socket{execIoc};
        acceptor.async_accept(socket, fibers::asio::yield[ec]);
        if (!ec) {
            socket.set_option(ip::tcp::no_delay(true));
            FiberPool::post(execIoc, &HttpSession::run,
                            std::make_shared<HttpSession>(shared_from_this(), std::move(socket)));
        }
    }
}

std::shared_ptr<WebsocketService> HttpService::getUpgrader(const std::string& path)
{
    for (auto& upgrader : m_upgraders) {
        if (upgrader->match(path)) {
            return upgrader;
        }
    }

    return nullptr;
}


}


