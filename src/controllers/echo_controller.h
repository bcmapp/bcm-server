#pragma once

#include <http/http_router.h>

namespace bcm {

class EchoController : public std::enable_shared_from_this<EchoController>
                     , public HttpRouter::Controller {
public:
    EchoController();
    ~EchoController();

    void addRoutes(HttpRouter& router) override;

public:
    void echo(HttpContext& context);

};

}

