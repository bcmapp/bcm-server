#include "lbs_register.h"
#include <utils/jsonable.h>
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <utils/log.h>
#include <fiber/asio_yield.h>
#include <utils/time.h>

namespace bcm {

using namespace boost;
namespace http = boost::beast::http;

struct LbsServiceInfo {
    std::string name;
    std::string ip;
    std::string port;
};

void to_json(nlohmann::json& j, const LbsServiceInfo& config)
{
    j = nlohmann::json{{"name", config.name},
                       {"ip", config.ip},
                       {"port", config.port}};
}

LbsRegister::LbsRegister(LbsConfig lbsConfig, ServiceConfig serviceConfig)
    : m_config(std::move(lbsConfig))
{
    for (auto& ip : serviceConfig.ips) {
        LbsServiceInfo service;
        service.name = m_config.name;
        service.ip = ip;
        service.port = std::to_string(serviceConfig.port);
        m_services.push_back(jsonable::toPrintable(service));
    }
}

LbsRegister::~LbsRegister()
{
}

void LbsRegister::run()
{
    int64_t start = nowInMilli();
    if (!m_isRegistered) {
        m_isRegistered = true;
        invokeLbsApi(REGISTER);
    } else {
        invokeLbsApi(KEEP_ALIVE);
    }
    m_execTime = nowInMilli() - start;
}

void LbsRegister::cancel()
{
    invokeLbsApi(UNREGISTER);
}

int64_t LbsRegister::lastExecTimeInMilli()
{
    return m_execTime;
}

void LbsRegister::invokeLbsApi(InvokeType type)
{
    asio::io_context* ioc = FiberPool::getThreadIOContext();

    if (ioc == nullptr) {
        LOGE << "isn't excuting in a fiber timer";
        return;
    }

    system::error_code ec;
    tcp::socket socket{*ioc};
    tcp::resolver resolver{*ioc};

    auto results = resolver.async_resolve(m_config.host, std::to_string(m_config.port), fibers::asio::yield[ec]);
    if (ec || results.empty()) {
        LOGE << "no resolve result: " << ec.message();
        return;
    }

    asio::async_connect(socket, results.begin(), results.end(), fibers::asio::yield[ec]);
    if (ec) {
        LOGE << "tcp connect error: " << ec.message();
        return;
    }

    http::request<http::string_body> req;
    req.version(11);
    req.target(m_config.target);
    req.set(http::field::host, m_config.host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.set(http::field::content_type, "application/json");

    std::string action;
    switch (type) {
        case REGISTER:
            req.method(http::verb::post);
            action = "register";
            break;
        case KEEP_ALIVE:
            req.method(http::verb::put);
            action = "keep alive";
            break;
        case UNREGISTER:
            req.method(http::verb::delete_);
            action = "unregister";
            break;
    }

    static uint64_t s_logLimiter = 0;

    for (auto& service : m_services) {
        req.set(http::field::content_length, service.size());
        req.body() = service;

        http::async_write(socket, req, fibers::asio::yield[ec]);
        if (ec) {
            LOGE << "send " << action << " request failed: " << ec.message();
            return;
        }

        http::response<http::string_body> res;
        beast::flat_buffer buffer;
        http::async_read(socket, buffer, res, fibers::asio::yield[ec]);
        if (ec) {
            LOGE << "read " << action << " response error: " << ec.message();
            return;
        }

        if (res.result() != http::status::ok) {
            LOGW << action << " response result error: " << res.result();
            return;
        }

        // print logs every 100 times
        if (s_logLimiter % 100 == 0) {
            LOGI << action << " service: " << service;
        }
    }

    ++s_logLimiter;
}

}