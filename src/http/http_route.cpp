#include "http_route.h"
#include "http_statics.h"
#include <utils/log.h>
#include <crypto/url_encoder.h>

namespace bcm {

HttpRoute::Segment::Segment(const std::string& raw)
    : m_type(Type::NONE)
    , m_raw(raw)
{
    init(m_raw);
}

bool HttpRoute::Segment::match(const std::string& raw) const
{
    if (isNone()) {
        return false;
    }

    if (isFixed()) {
        return raw == m_raw;
    }

    return true;
}

bool HttpRoute::Segment::match(const HttpRoute::Segment& other) const
{
    return match(other.raw());
}

std::vector<HttpRoute::Segment> HttpRoute::Segment::fromPath(const std::string& path)
{
    std::vector<HttpRoute::Segment> segments;

    std::istringstream iss(path);
    std::string p;

    while (std::getline(iss, p, '/')) {
        if (!p.empty()) {
            segments.emplace_back(p);
        }
    }

    return segments;
}

void HttpRoute::Segment::init(const std::string& raw)
{
    if (raw[0] == ':') {
        m_type = Type::PARAMETER;
    } else if (raw[0] == '*') {
        if (raw.size() > 1) {
            // LOG(ERROR) << "splat should be single";
            return;
        }
        m_type = Type::SPLAT;
    } else {
        m_type = Type::FIXED;
    }

    m_raw = raw;
}

HttpRoute::HttpRoute(http::verb verb, const std::string& path, Authenticator::AuthType type, Handler handler,
                     JsonSerializer* reqSerializer, JsonSerializer* resSerializer)
    : m_verb(verb)
    , m_path(path)
    , m_authType(type)
    , m_handler(std::move(handler))
    , m_reqSerializer(reqSerializer)
    , m_resSerializer(resSerializer)
    , m_segments(Segment::fromPath(path))
{
}

bool HttpRoute::match(http::verb verb, const std::string& path, std::map<std::string, std::string>& params) const
{
    if (m_handler == nullptr) {
        return false;
    }

    if (verb != m_verb) {
        return false;
    }

    std::vector<Segment> reqSegments = Segment::fromPath(path);
    if (reqSegments.size() != m_segments.size()) {
        return false;
    }

    for (std::vector<Segment>::size_type i = 0; i < m_segments.size(); ++i) {
        const auto& seg = m_segments[i];
        const auto& reqSeg = reqSegments[i];

        if (!seg.match(reqSeg)) {
            return false;
        }

        std::string tmp = UrlEncoder::decode(reqSeg.raw());
        if (seg.isParameter()) {
            params[seg.raw()] = tmp;
        } else if (seg.isSplat()) {
            params[tmp] = tmp;
        }
    }
    return true;
}

void HttpRoute::invokeHandler(HttpContext& context)
{
    auto& req = context.request;
    auto& res = context.response;
    auto& statics = context.statics;

    LOGT << "req body: " << req.body();

    try {
        if (m_reqSerializer) {
            if (req.body().empty()) {
                res.result(http::status::bad_request);
                res.body() = "Body cannot be empty";

                statics.setStatus(http::status::bad_request);
                statics.setMessage("body is empty");
                return;
            }

            if (req[http::field::content_type].find("application/json") == std::string::npos) {
                res.result(http::status::bad_request);
                res.body() = "The content-type should be application/json";

                statics.setStatus(http::status::bad_request);
                statics.setMessage("content-type is not application/json");
                return;
            }

            if (!m_reqSerializer->deserialize(req.body(), context.requestEntity)) {
                res.result(http::status::bad_request);
                res.body() = "The body deserialize failed!";

                statics.setStatus(http::status::bad_request);
                statics.setMessage("request body deserialize failed");
                return;
            }
        }

        m_handler(context);

        if (!context.responseEntity.empty()) {
            BOOST_ASSERT_MSG(m_resSerializer != nullptr, "miss a response serializer");
            if (!m_resSerializer) {
                res.result(http::status::internal_server_error);
                res.body() = "miss a serializer of response!";

                statics.setStatus(http::status::internal_server_error);
                statics.setMessage("response serialize failed");
                statics.setLogLevel(LOGSEVERITY_ERROR);
                return;
            }

            std::string payload;
            if (!m_resSerializer->serialize(context.responseEntity, payload)) {
                BOOST_ASSERT_MSG(0, "response serialize failed");
                res.result(http::status::internal_server_error);
                res.body() = "serialize response failed!";

                statics.setStatus(http::status::internal_server_error);
                statics.setMessage("response serialize failed");
                statics.setLogLevel(LOGSEVERITY_ERROR);
                return;
            }

            res.set(http::field::content_type, "application/json; charset=utf-8");
            res.set(http::field::content_length, payload.size());
            res.body() = payload;

            if (res.result_int() == static_cast<unsigned>(http::status::unknown)) {
                res.result(http::status::ok);
            }
        }

        if (res.result_int() == static_cast<unsigned>(http::status::unknown)) {
            res.result(http::status::internal_server_error);

            statics.setStatus(http::status::internal_server_error);
            statics.setMessage("should set a response status");
            statics.setLogLevel(LOGSEVERITY_ERROR);
        }
    } catch (std::exception& e) {
        res.result(http::status::internal_server_error);

        statics.setStatus(http::status::internal_server_error);
        statics.setMessage("unhandle exception: " + std::string(e.what()));
        statics.setLogLevel(LOGSEVERITY_ERROR);
        return;
    }
    statics.setStatus(res.result_int());
    LOGT << "res body: " << res.body();
}

}
