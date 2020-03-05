#pragma once

#include "http_route.h"
#include <boost/optional.hpp>
#include "common/observer.h"
#include "common/api.h"

namespace bcm {

class HttpRouter : Observable<Api> {
public:
    class Controller {
    public:
        virtual void addRoutes(HttpRouter& router) = 0;
    };

    class Filter {
    public:
        // status and reason is for self-define http response status code and reason phrase
        virtual bool onFilter(const http::request<http::string_body>& header, uint32_t& status, std::string& reason) = 0;
    };

    enum MatchStatus {
        MATCHED = 1,
        FILTERED,
        MISMATCHED,
    };

    struct MatchResult {
        MatchStatus matchStatus{MatchStatus::MISMATCHED};
        struct {
            // init with default value
            uint32_t status{460};
            std::string reason{"Resource Restricted by Filter"};
        } filter;
        std::shared_ptr<HttpRoute> matchedRoute;
    };

    void add(std::shared_ptr<Controller> controller);
    void add(http::verb verb, const std::string& path, Authenticator::AuthType  auth, HttpRoute::Handler handler,
             JsonSerializer* reqSerializer = nullptr, JsonSerializer* resSerializer = nullptr);
    void add(std::shared_ptr<Filter> filter) { m_filters.emplace_back(std::move(filter)); };

    MatchResult match(HttpContext& context);
    MatchResult match(const http::request<http::string_body>& header,
                      std::map<std::string, std::string>& queryParams,
                      std::map<std::string, std::string>& pathParams);

    static void parseUri(const std::string& uri, std::string& target, std::map<std::string, std::string>& queryParams);

    virtual void registerObserver(const std::shared_ptr<ObserverType>& observer) override;
    virtual void unregisterObserver(const std::shared_ptr<ObserverType>& observer) override;
    virtual void notify(const Api& api) override;

private:
    std::vector<std::shared_ptr<Controller>> m_controllers;
    std::map<std::string, std::shared_ptr<HttpRoute>> m_routes;
    std::vector<std::shared_ptr<Filter>> m_filters;
    std::set<std::shared_ptr<ObserverType>> m_observers;
};

}




