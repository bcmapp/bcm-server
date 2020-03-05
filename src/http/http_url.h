#pragma once

#include <string>

namespace bcm {

class HttpUrl {
public:
    HttpUrl(const std::string& url);

    ~HttpUrl() = default;

    bool invalid() { return m_bInvalid; }
    std::string protocol() { return m_protocol; }
    std::string host() { return m_host; }
    bool isIPv6() { return m_bIPv6Host; }
    int port();
    std::string path() { return m_path; }
    std::string query() { return m_query; }


private:
    void parse(const char* url);
    bool unescapePath(const std::string& in, std::string& out);

private:
    bool m_bInvalid{false};
    std::string m_protocol;
    std::string m_host;
    bool m_bIPv6Host{false};
    std::string m_port;
    std::string m_path;
    std::string m_query;
};

}