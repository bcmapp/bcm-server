#pragma once

#include "http_router.h"
#include "http_service.h"
#include <boost/asio/ssl.hpp>

namespace bcm {

namespace asio = boost::asio;
namespace ip = boost::asio::ip;
namespace ssl = boost::asio::ssl;

using WebsocketStrem = websocket::stream<ssl::stream<ip::tcp::socket>>;

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(std::shared_ptr<HttpService> owner, ip::tcp::socket socket);
    ~HttpSession();

    void run();

private:
    bool sendResponse(http::response<http::string_body>& response);
    void upgrade(http::request<http::string_body>& header);
    boost::optional<HttpRoute&> onHeader(http::request<http::string_body>& header, HttpContext& context);
    bool onBody(http::request<http::string_body>& request, HttpContext& context);

private:
    std::shared_ptr<HttpService> m_service;
    std::shared_ptr<WebsocketStrem> m_stream;
};

};

