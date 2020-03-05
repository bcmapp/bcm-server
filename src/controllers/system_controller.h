#pragma once

#include <http/http_router.h>
#include "store/accounts_manager.h"
#include "push/push_service.h"
#include "dao/client.h"
#include "config/sysmsg_config.h"

namespace bcm {

class InnerSystemController : public std::enable_shared_from_this<InnerSystemController>
                            , public HttpRouter::Controller {
public:
    InnerSystemController(std::shared_ptr<AccountsManager> ptrAccountsManager,
                     std::shared_ptr<PushService> pushService,
                     std::shared_ptr<dao::SysMsgs> sysMsgs,
                     const SysMsgConfig& conf);
    ~InnerSystemController() = default;

    void addRoutes(HttpRouter& router) override;

public:
    void sendSystemOfflinePush(HttpContext& context);

private:
    std::shared_ptr<AccountsManager> m_ptrAccountsManager;
    std::shared_ptr<push::Service> m_pushService;
    std::shared_ptr<dao::SysMsgs> m_sysMsgs;
    bool m_openService = false;
};

class SystemController : public std::enable_shared_from_this<SystemController>
                       , public HttpRouter::Controller {
public:
    explicit SystemController(std::shared_ptr<dao::SysMsgs> sysMsgs);
    ~SystemController() = default;

    void addRoutes(HttpRouter& router) override;

public:
    void getSystemMsgs(HttpContext& context);
    void ackSystemMsgs(HttpContext& context);

private:
    std::shared_ptr<dao::SysMsgs> m_sysMsgs;
};

}
