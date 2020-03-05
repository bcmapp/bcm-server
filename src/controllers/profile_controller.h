#pragma once

#include "http/http_router.h"
#include "store/accounts_manager.h"
#include "auth/authenticator.h"

namespace bcm {

class ProfileController : public std::enable_shared_from_this<ProfileController>
                        , public HttpRouter::Controller {

public:

    ProfileController(std::shared_ptr<AccountsManager> accountsManager,
                      std::shared_ptr<Authenticator> authenticator);

    ~ProfileController() = default;

    void addRoutes(HttpRouter& router) override;

public:

    void getSingleProfile(HttpContext& context);
    void getProfileBatch(HttpContext& context);
    void setName(HttpContext& context);
    void setAvatar(HttpContext& context);
    void setPrivacy(HttpContext& context);
    void uploadGroupAvatar(HttpContext& context);
    void getAvatar(HttpContext& context);
    void setVersion(HttpContext& context);

    void setNickname(HttpContext& context);
    void setAvatar2(HttpContext& context);
    void setKeys(HttpContext& context);
    void getKeys(HttpContext& context);

private:

    std::string generateUUID();

private:
    std::shared_ptr<AccountsManager> m_accountsManager;
    std::shared_ptr<Authenticator> m_authenticator;

private:
    static const std::string kAvatarPrefix;
    static const std::string kDeletedAccountName;
};

} // namespace bcm
