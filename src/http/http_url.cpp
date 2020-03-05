#include "http_url.h"
#include <cstring>
#include <algorithm>

namespace bcm {

HttpUrl::HttpUrl(const std::string& url)
{
    parse(url.c_str());
}

int HttpUrl::port()
{
    if (!m_port.empty()) {
        return std::stoi(m_port);
    }
    if (m_protocol == "http") {
        return 80;
    }
    if (m_protocol == "https") {
        return 443;
    }
    return 0;
}

void HttpUrl::parse(const char* url)
{
    m_bInvalid = true;

    // Protocol.
    std::size_t length = std::strcspn(url, ":");
    m_protocol.assign(url, url + length);
    for (char& c : m_protocol) {
        c = std::tolower(c);
    }
    url += length;

    // "://".
    if (*url++ != ':') {
        return;
    }
    if (*url++ != '/') {
        return;
    }
    if (*url++ != '/') {
        return;
    }

    // UserInfo.
    /*length = std::strcspn(url, "@:[/?#");
    if (url[length] == '@') {
        m_userInfo.assign(url, url + length);
        url += length + 1;
    }
    else if (url[length] == ':') {
        std::size_t length2 = std::strcspn(url + length, "@/?#");
        if (url[length + length2] == '@') {
            m_userInfo.user_info_.assign(url, url + length + length2);
            url += length + length2 + 1;
        }
    }*/

    // Host.
    if (*url == '[') {
        length = std::strcspn(++url, "]");
        if (url[length] != ']') {
            return;
        }
        m_host.assign(url, url + length);
        m_bIPv6Host = true;
        url += length + 1;
        if (std::strcspn(url, ":/?#") != 0) {
            return;
        }
    } else {
        length = std::strcspn(url, ":/?#");
        m_host.assign(url, url + length);
        url += length;
    }

    // Port.
    if (*url == ':') {
        length = std::strcspn(++url, "/?#");
        if (length == 0) {
            return;
        }
        m_port.assign(url, url + length);
        for (char& c : m_port) {
            if (!std::isdigit(c)) {
                return;
            }
        }
        url += length;
    }

    // Path.
    if (*url == '/') {
        length = std::strcspn(url, "?#");
        m_path.assign(url, url + length);
        std::string tmp_path;
        if (!unescapePath(m_path, tmp_path)) {
            return;
        }
        url += length;
    } else {
        m_path = "/";
    }

    // Query.
    if (*url == '?') {
        length = std::strcspn(++url, "#");
        m_query.assign(url, url + length);
        url += length;
    }

    // Fragment.
    /*if (*url == '#') {
        m_fragment.assign(++url);
    }*/
    m_bInvalid = false;
}

bool HttpUrl::unescapePath(const std::string& in, std::string& out)
{
    out.clear();
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        switch (in[i]) {
            case '%':
                if (i + 3 <= in.size()) {
                    unsigned int value = 0;
                    for (std::size_t j = i + 1; j < i + 3; ++j) {
                        switch (in[j]) {
                            case '0': case '1': case '2': case '3': case '4':
                            case '5': case '6': case '7': case '8': case '9':
                                value += in[j] - '0';
                                break;
                            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
                                value += in[j] - 'a' + 10;
                                break;
                            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
                                value += in[j] - 'A' + 10;
                                break;
                            default:
                                return false;
                        }
                        if (j == i + 1) {
                            value <<= 4U;
                        }
                    }
                    out += static_cast<char>(value);
                    i += 2;
                } else {
                    return false;
                }
                break;
            case '-': case '_': case '.': case '!': case '~': case '*':
            case '\'': case '(': case ')': case ':': case '@': case '&':
            case '=': case '+': case '$': case ',': case '/': case ';':
                out += in[i];
                break;
            default:
                if (!std::isalnum(in[i])) {
                    return false;
                }
                out += in[i];
                break;
        }
    }
    return true;
}

}
