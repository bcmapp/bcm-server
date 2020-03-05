#include "test_common.h"

#include <http/http_router.h>

using namespace bcm;

class TestController : public HttpRouter::Controller {
public:
    TestController() = default;
    void onTest(HttpContext&) {}

    void addRoutes(HttpRouter& router) override {
        router.add(http::verb::get, "/test/:content", Authenticator::AUTHTYPE_NO_AUTH,
                   std::bind(&TestController::onTest, this, std::placeholders::_1));
        router.add(http::verb::put, "/test/:address/:message", Authenticator::AUTHTYPE_ALLOW_ALL,
                   std::bind(&TestController::onTest, this, std::placeholders::_1));
    }
};

TEST_CASE("Router") {

    auto controller = std::make_shared<TestController>();
    auto router = std::make_shared<HttpRouter>();

    router->add(controller);

    HttpContext context;
    auto& request = context.request;
    auto& pathParams = context.pathParams;
    auto& queryParams = context.queryParams;

    request.target("/test/echo?add=100");
    request.method(http::verb::get);
    auto result = router->match(context);
    REQUIRE(result.matchStatus == HttpRouter::MATCHED);
    REQUIRE(result.matchedRoute->getAuthType() == Authenticator::AUTHTYPE_NO_AUTH);
    REQUIRE(pathParams.at(":content") == "echo");
    REQUIRE(queryParams.at("add") == "100");

    queryParams.clear();
    pathParams.clear();
    request.target("/test/echo/");
    result = router->match(context);
    REQUIRE(result.matchStatus == HttpRouter::MATCHED);
    REQUIRE(result.matchedRoute->getAuthType() == Authenticator::AUTHTYPE_NO_AUTH);
    REQUIRE(pathParams.at(":content") == "echo");

    queryParams.clear();
    pathParams.clear();
    request.method(http::verb::put);
    result = router->match(context);
    REQUIRE(result.matchStatus == HttpRouter::MISMATCHED);

    queryParams.clear();
    pathParams.clear();
    request.target("/test/bob/hello/?class=test");
    result = router->match(context);
    REQUIRE(result.matchStatus == HttpRouter::MATCHED);
    REQUIRE(result.matchedRoute->getAuthType() == Authenticator::AUTHTYPE_ALLOW_ALL);
    REQUIRE(pathParams.at(":address") == "bob");
    REQUIRE(pathParams.at(":message") == "hello");
    REQUIRE(queryParams.at("class") == "test");

}

