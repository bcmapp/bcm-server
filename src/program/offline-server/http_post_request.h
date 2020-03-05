#pragma once
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>

#include <nlohmann/json.hpp>


#include "offline_server_entities.h"

namespace bcm {
    
    namespace ip = boost::asio::ip;
    namespace ssl = boost::asio::ssl;
    namespace http = boost::beast::http;
    
    class HttpPostRequest : public std::enable_shared_from_this<HttpPostRequest> {
        ip::tcp::resolver m_resolver;
        ssl::stream<ip::tcp::socket> m_stream;
        boost::beast::flat_buffer m_buffer;
        http::request<http::string_body> m_req;
        http::response<http::string_body> m_res;
        std::string m_host;
        std::string m_port;
        std::string m_webPath;
        
    public:
        typedef std::shared_ptr<HttpPostRequest> shared_ptr;
    
        HttpPostRequest(boost::asio::io_context& ioc, ssl::context& ctx, const std::string& url)
                : m_resolver(ioc)
                , m_stream(ioc, ctx)
                , m_port("80")
                , m_webPath(url)
        {
            m_req.method(http::verb::post);
            m_req.target(m_webPath);
            m_req.set(http::field::content_type, "application/json");
        }
        
        shared_ptr setServerAddr(const std::string& addr)
        {
            std::vector<std::string> tokens;
            boost::split(tokens, addr, boost::is_any_of(":"));
            
            if (tokens.size() == 1) {
                m_host = std::move(tokens[0]);
            } else if (tokens.size() >= 2) {
                m_host = std::move(tokens[0]);
                m_port = std::move(tokens[1]);
            }
            
            return shared_from_this();
        }
        
        shared_ptr setPostData(const std::string& data)
        {
            m_req.set(http::field::content_length, data.size());
            m_req.body().assign(data);
            return shared_from_this();
        }
        
        void exec()
        {
            if (!SSL_set_tlsext_host_name(m_stream.native_handle(),
                                          m_host.c_str())) {
                LOGE << "SSL_set_tlsext_host_name error: " << ERR_get_error()
                     << ", host: " << m_host
                     << ", port: " << m_port
                     << ", web path: " << m_webPath;
                return;
            }
            
            m_resolver.async_resolve(m_host, m_port,
                                     std::bind(&HttpPostRequest::onResolve, shared_from_this(),
                                               std::placeholders::_1, std::placeholders::_2));
        }
    
    private:
        void onResolve(boost::beast::error_code ec,
                       ip::tcp::resolver::results_type results)
        {
            if (ec) {
                LOGE << "resolve to: " << m_host << ":" << m_port
                     << ", error: " << ec << ", body: " << m_req.body();
                return;
            }
            
            boost::asio::async_connect(m_stream.next_layer(),
                                       results.begin(), results.end(),
                                       std::bind(&HttpPostRequest::onConnect, shared_from_this(),
                                                 std::placeholders::_1));
        }
        
        void onConnect(boost::beast::error_code ec)
        {
            if (ec) {
                LOGE << "connect to: " << m_host << ":" << m_port
                     << ", error: " << ec << ", body: " << m_req.body();
                return;
            }
            
            m_stream.async_handshake(ssl::stream_base::client,
                                     std::bind(&HttpPostRequest::onHandshake, shared_from_this(),
                                               std::placeholders::_1));
        }
        
        void onHandshake(boost::beast::error_code ec)
        {
            if (ec) {
                LOGE << "handshake with server: " << m_host << ":" << m_port
                     << ", error: " << ec << ", body: " << m_req.body();
                return;
            }
            
            http::async_write(m_stream, m_req,
                              std::bind(&HttpPostRequest::onWrite, shared_from_this(),
                                        std::placeholders::_1, std::placeholders::_2));
        }
        
        void onWrite(boost::beast::error_code ec, std::size_t bytes_transferred)
        {
            boost::ignore_unused(bytes_transferred);
            if (ec) {
                LOGE << "write data to: " << m_host << ":" << m_port
                     << ", error: " << ec << ", body: " << m_req.body();
                return;
            }
            
            http::async_read(m_stream, m_buffer, m_res,
                             std::bind(&HttpPostRequest::onRead, shared_from_this(),
                                       std::placeholders::_1, std::placeholders::_2));
        }
        
        void onRead(boost::beast::error_code ec, std::size_t bytes_transferred)
        {
            boost::ignore_unused(bytes_transferred);
            if (ec) {
                LOGE << "read reply from: " << m_host << ":" << m_port
                     << ", error: " << ec << ", body: " << m_req.body();
                return;
            }
            
            LOGI << "reply from server: " << m_host << ":" << m_port
                 << ", response: " << m_res.body() << ", request: " << m_req.body();
            m_stream.async_shutdown(
                    std::bind(&HttpPostRequest::onShutdown, shared_from_this(),
                              std::placeholders::_1));
        }
        
        void onShutdown(boost::beast::error_code ec)
        {
            LOGI << "shutdown connection to: " << m_host << ":" << m_port
                 << ", error_code: " << ec;
        }
    };
} // namespace bcm