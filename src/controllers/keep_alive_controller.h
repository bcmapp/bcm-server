#pragma once

#include <http/http_router.h>
#include <boost/beast/http/status.hpp>
#include <dispatcher/dispatch_manager.h>

namespace bcm {

class GroupMsgService;

class KeepAliveController
    : public HttpRouter::Controller
    , public std::enable_shared_from_this<KeepAliveController> {
public:
    KeepAliveController(std::shared_ptr<DispatchManager> dispatchManager,
                        std::shared_ptr<GroupMsgService> groupMsgService);
    ~KeepAliveController();

public:
    void addRoutes(HttpRouter& router) override;

    void getKeepAlive(HttpContext& context);

private:
    std::shared_ptr<DispatchManager> m_dispatchManager;
    std::shared_ptr<GroupMsgService> m_groupMsgService;
};

}

