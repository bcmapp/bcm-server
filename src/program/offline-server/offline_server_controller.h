#pragma once

#include <boost/beast/http/status.hpp>
#include "http/http_router.h"
#include "offline_server_entities.h"
#include "dispatcher/offline_dispatcher.h"
#include "../../config/offline_server_config.h"

namespace bcm {
namespace push {
    class Service;
} // namesapce push
} // namespace bcm

namespace bcm {
    
class OfflinePushController
        : public HttpRouter::Controller
                , public std::enable_shared_from_this<OfflinePushController> {
public:
    OfflinePushController(std::shared_ptr<push::Service> pushService, OfflineServerConfig config);
    ~OfflinePushController();

public:
    void addRoutes(HttpRouter& router) override;

    void sendGroupOfflinePush(HttpContext& context);
    void dispatchNotifications(HttpContext& context);

private:
    std::shared_ptr<push::Service> m_pushService;
    OfflineServerConfig m_config;
};
    
} // namespace bcm

