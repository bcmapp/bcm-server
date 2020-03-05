#pragma once

#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <http/http_url.h>
#include <utils/log.h>
#include <fiber/asio_yield.h>
#include <utils/ssl_utils.h>

namespace bcm {

namespace ip = boost::asio::ip;
namespace ssl = boost::asio::ssl;
namespace http = boost::beast::http;

template <http::verb method>
class HttpClient {
public:
    explicit HttpClient(const std::string& url);
    ~HttpClient() = default;

    HttpClient& header(const std::string& key, const std::string& value);
    HttpClient& header(http::field key, const std::string& value);
    HttpClient& body(const std::string& type, const std::string& content);

    bool process(boost::asio::io_context& ioc);
    http::response<http::string_body>& response() { return m_res; }

private:
    bool processHttp(boost::asio::io_context& ioc);
    bool processHttps(boost::asio::io_context& ioc, ssl::context& sslc);

private:
    HttpUrl m_url;
    http::request<http::string_body> m_req;
    http::response<http::string_body> m_res;
};

template <http::verb method>
HttpClient<method>::HttpClient(const std::string& url)
    : m_url(url)
{
    m_req.method(method);
    m_req.version(11);
    m_req.target(m_url.path() + m_url.query());
    m_req.set(http::field::host, m_url.host());
    m_req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
}

template <http::verb method>
HttpClient<method> & HttpClient<method>::header(http::field key, const std::string& value)
{
    m_req.set(key, value);
    return *this;
}

template <http::verb method>
HttpClient<method>& HttpClient<method>::header(const std::string& key, const std::string& value)
{
    m_req.set(key, value);
    return *this;
}

template <http::verb method>
HttpClient<method>& HttpClient<method>::body(const std::string& type, const std::string& content)
{
    m_req.set(http::field::content_type, type);
    m_req.set(http::field::content_length, content.length());
    m_req.body() = content;
    return *this;
}

template <http::verb method>
bool HttpClient<method>::process(boost::asio::io_context& ioc)
{
    if (m_url.invalid()) {
        return false;
    }

    if (m_url.protocol() == "https") {
        return processHttps(ioc, SslUtils::getGlobalClientContext());
    } else if (m_url.protocol() == "http") {
        return processHttp(ioc);
    } else {
        return false;
    }
}

template <http::verb method>
bool HttpClient<method>::processHttp(boost::asio::io_context& ioc)
{
    boost::system::error_code ec;
    ip::tcp::resolver resolver{ioc};
    ip::tcp::socket stream{ioc};

    auto results = resolver.async_resolve(m_url.host(), std::to_string(m_url.port()), fibers::asio::yield[ec]);
    if (ec || results.empty()) {
        LOGE << "no resolve result: " << ec.message();
        return false;
    }

    asio::async_connect(stream, results.begin(), results.end(), fibers::asio::yield[ec]);
    if (ec) {
        LOGE << "tcp connect error: " << ec.message();
        return false;
    }

    http::async_write(stream, m_req, fibers::asio::yield[ec]);
    if (ec) {
        LOGE << "send request: " << ec.message();
        return false;
    }

    boost::beast::flat_buffer buffer;
    http::async_read(stream, buffer, m_res, fibers::asio::yield[ec]);
    if (ec) {
        LOGE << "read response: " << ec.message();
        return false;
    }
    return true;
}

template <http::verb method>
bool HttpClient<method>::processHttps(boost::asio::io_context& ioc, ssl::context& sslc)
{
    boost::system::error_code ec;
    ip::tcp::resolver resolver{ioc};
    ssl::stream<ip::tcp::socket> stream{ioc, sslc};

    if (!SSL_set_tlsext_host_name(stream.native_handle(), m_url.host().data())) {
        LOGE << "ssl connect error: " << ERR_get_error();
        return false;
    }

    auto results = resolver.async_resolve(m_url.host(), std::to_string(m_url.port()), fibers::asio::yield[ec]);
    if (ec || results.empty()) {
        LOGE << "no resolve result: " << ec.message();
        return false;
    }

    asio::async_connect(stream.next_layer(), results.begin(), results.end(), fibers::asio::yield[ec]);
    if (ec) {
        LOGE << "tcp connect error: " << ec.message();
        return false;
    }

    stream.async_handshake(ssl::stream_base::client, fibers::asio::yield[ec]);
    if (ec) {
        LOGE << "ssl handshake error: " << ec.message();
        return false;
    }

    http::async_write(stream, m_req, fibers::asio::yield[ec]);
    if (ec) {
        LOGE << "send request: " << ec.message();
        return false;
    }

    boost::beast::flat_buffer buffer;
    http::async_read(stream, buffer, m_res, fibers::asio::yield[ec]);
    if (ec) {
        LOGE << "read response: " << ec.message();
        return false;
    }
    return true;
}

using HttpGet = HttpClient<http::verb::get>;
using HttpPut = HttpClient<http::verb::put>;
using HttpPost = HttpClient<http::verb::post>;

}
