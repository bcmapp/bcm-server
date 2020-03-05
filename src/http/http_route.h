#pragma once

#include <string>
#include <map>
#include <vector>
#include <boost/beast.hpp>
#include <boost/any.hpp>
#include <utils/json_serializer.h>
#include "http_context.h"
#include "auth/authenticator.h"

namespace bcm {

namespace http = boost::beast::http;

class HttpRoute {
public:
    typedef std::function<void (HttpContext& context)> Handler;

    HttpRoute(http::verb verb, const std::string& path, Authenticator::AuthType type, Handler handler,
              JsonSerializer* reqSerializer = nullptr, JsonSerializer* resSerializer = nullptr);

    std::string name() { return  http::to_string(m_verb).to_string() + "_" + m_path; }
    Authenticator::AuthType getAuthType() { return m_authType; }
    bool match(http::verb verb, const std::string& path, std::map<std::string, std::string>& params) const;
    void invokeHandler(HttpContext& context);

private:
    // inspired by pistache
    class Segment {
    public:
        explicit Segment(const std::string& raw);

        bool match(const std::string& raw) const;
        bool match(const Segment& other) const;

        bool isNone() const { return  m_type == Type::NONE; }
        bool isFixed() const { return m_type == Type::FIXED; }
        bool isParameter() const { return m_type == Type::PARAMETER; }
        bool isSplat() const { return m_type == Type::SPLAT; };

        std::string raw() const { return m_raw; }
        static std::vector<Segment> fromPath(const std::string& path);

    private:
        enum class Type {
            NONE = 0,
            FIXED,      // normal string
            PARAMETER,  // start with ':'
            SPLAT,      // a single '*'
        };

        void init(const std::string& raw);

        Type m_type;
        std::string m_raw;
    };

private:
    http::verb m_verb;
    std::string m_path;
    Authenticator::AuthType m_authType;
    Handler m_handler;
    std::unique_ptr<JsonSerializer> m_reqSerializer;
    std::unique_ptr<JsonSerializer> m_resSerializer;
    std::vector<Segment> m_segments;
};

}



