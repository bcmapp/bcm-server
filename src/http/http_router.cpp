#include "http_router.h"
#include <crypto/url_encoder.h>
#include <boost/range/adaptor/reversed.hpp>

namespace bcm {

void HttpRouter::add(std::shared_ptr<Controller> controller)
{
    m_controllers.push_back(controller);
    controller->addRoutes(*this);
}

void HttpRouter::add(http::verb verb, const std::string& path, Authenticator::AuthType type, HttpRoute::Handler handler,
                     JsonSerializer* reqSerializer, JsonSerializer* resSerializer)
{
    auto route = std::make_shared<HttpRoute>(verb, path, type, std::move(handler), reqSerializer, resSerializer);
    m_routes[route->name()] = route;
    notify(Api(verb, path));
}

HttpRouter::MatchResult HttpRouter::match(HttpContext& context)
{
    return match(context.request, context.queryParams, context.pathParams);
}

HttpRouter::MatchResult HttpRouter::match(const http::request<http::string_body>& header,
                                          std::map<std::string, std::string>& queryParams,
                                          std::map<std::string, std::string>& pathParams)
{
    MatchResult matchResult;
    std::string path;
    parseUri(header.target().to_string(), path, queryParams);

    for (auto& filter : m_filters) {
        if (filter->onFilter(header, matchResult.filter.status, matchResult.filter.reason)) {
            matchResult.matchStatus = FILTERED;
            return matchResult;
        }
    }

    // Reverse traversal for matching fixed route-segment firstly.
    // The key of routes map is sorted by ascii character table,
    // where `:`/`*` is less than letter, by default, and
    // fixed route-segment is always begin with a letter.
    for (auto& pair : boost::adaptors::reverse(m_routes)) {
        if (pair.second->match(header.method(), path, pathParams)) {
            matchResult.matchStatus = MATCHED;
            matchResult.matchedRoute = pair.second;
            return matchResult;
        }
    }
    matchResult.matchStatus = MISMATCHED;
    return matchResult;
}

void HttpRouter::parseUri(const std::string& uri, std::string& target,
                          std::map<std::string, std::string>& queryParams)
{
    auto queryStartPos = uri.find('?');
    if (queryStartPos == std::string::npos) {
        target = uri;
        return;
    }

    target = uri.substr(0, queryStartPos);
    std::string query = uri.substr(queryStartPos + 1).append("&");

    std::string key;
    std::string value;
    std::string::size_type lastPos =  0;
    for (auto pos = lastPos; pos < query.length(); ++pos) {
        if (query[pos] == '=') {
            key = query.substr(lastPos, pos - lastPos);
            lastPos = pos + 1;
        } else if (query[pos] == '&') {
            value = query.substr(lastPos, pos - lastPos);
            lastPos = pos + 1;

            if (!key.empty() && !value.empty()) {
                queryParams[key] = UrlEncoder::decode(value);
            }
            key.clear();
            value.clear();
        }
    }

}

void HttpRouter::registerObserver(const std::shared_ptr<ObserverType>& observer)
{
    m_observers.emplace(observer);
}

void HttpRouter::unregisterObserver(const std::shared_ptr<ObserverType>& observer)
{
    m_observers.erase(observer);
}

void HttpRouter::notify(const Api& api)
{
    for (const auto& item : m_observers) {
        if (item != nullptr) {
            item->update(api);
        }
    }
}

}
