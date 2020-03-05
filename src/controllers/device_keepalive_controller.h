#pragma once

#include <http/http_router.h>
#include <boost/beast/http/status.hpp>
#include <dispatcher/dispatch_manager.h>

namespace bcm {

class DeviceKeepaliveController
    : public HttpRouter::Controller
    , public std::enable_shared_from_this<DeviceKeepaliveController> {
public:
    DeviceKeepaliveController(std::shared_ptr<DispatchManager> dispatchManager);
    ~DeviceKeepaliveController();

public:
    void addRoutes(HttpRouter& router) override;

    void getKeepAlive(HttpContext& context);
    void getProvisioningKeepAlive(HttpContext& context);
    void deviceKeepAlive(HttpContext& context);

private:
    std::shared_ptr<DispatchManager> m_dispatchManager;
};

}

