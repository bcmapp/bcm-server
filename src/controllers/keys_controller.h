#pragma once

#include <http/http_router.h>
#include <store/accounts_manager.h>
#include <store/keys_manager.h>
#include "config/size_check_config.h"


namespace bcm {

class KeysController 
    : public HttpRouter::Controller
    , public std::enable_shared_from_this<KeysController> {
public:
    KeysController(std::shared_ptr<AccountsManager> accountsManager,
                   std::shared_ptr<KeysManager> keysManager,
                   const SizeCheckConfig& scCfg);
    ~KeysController();

    void addRoutes(HttpRouter& router) override;

private:
    void getStatus(HttpContext& context);
    void setKeys(HttpContext& context);
    void getDeviceKeys(HttpContext& context);
    void setSignedKey(HttpContext& context);
    void getSignedKey(HttpContext& context);

private:
    int getAccount(const std::string& uid, const std::string& deviceSelector, Account& account);

private:
    std::shared_ptr<AccountsManager> m_accountsManager;
    std::shared_ptr<KeysManager> m_keysManager;
    SizeCheckConfig m_sizeCheckConfig;
};

}

