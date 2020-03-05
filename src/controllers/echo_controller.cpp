#include "echo_controller.h"
#include <http/http_router.h>

namespace bcm {


EchoController::EchoController() {}

EchoController::~EchoController() {}

void EchoController::addRoutes(bcm::HttpRouter& router)
{
    router.add(http::verb::get, "/echo/:something", Authenticator::AUTHTYPE_NO_AUTH,
               std::bind(&EchoController::echo, shared_from_this(), std::placeholders::_1));
}

void EchoController::echo(HttpContext& context)
{
    auto& response = context.response;
    response.result(http::status::ok);
    response.body() = context.pathParams.at(":something");
}

}

