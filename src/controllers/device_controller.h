#pragma once

#include <http/http_router.h>
#include <dao/client.h>
#include <store/accounts_manager.h>
#include <store/keys_manager.h>
#include <dispatcher/dispatch_manager.h>
#include "config/multi_device_config.h"

namespace bcm {

class DeviceController : public std::enable_shared_from_this<DeviceController>
                         , public HttpRouter::Controller {
public:
    DeviceController(std::shared_ptr<AccountsManager> accountsManager,
                       std::shared_ptr<DispatchManager> dispatchManager,
                       std::shared_ptr<KeysManager> keysManager,
                       const MultiDeviceConfig& multiDeviceConfig);

    ~DeviceController() = default;

    void addRoutes(HttpRouter& router) override;

public:
    void signin(HttpContext& context);

    void getDevices(HttpContext& context);
    void manageDevice(HttpContext& context);
    void requestLogin(HttpContext& context);
    void getLoginRequestInfo(HttpContext& context);
    void syncAvatar(HttpContext& context);
    void authorizeDevice(HttpContext& context);
    void logout(HttpContext& context);

private:
    std::shared_ptr<AccountsManager> m_accountsManager;
    std::shared_ptr<DispatchManager> m_dispatchManager;
    std::shared_ptr<KeysManager> m_keysManager;
    MultiDeviceConfig m_multiDeviceConfig;
};

}
