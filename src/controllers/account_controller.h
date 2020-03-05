#pragma once

#include <http/http_router.h>
#include <dao/client.h>
#include <store/accounts_manager.h>
#include <store/keys_manager.h>
#include <store/contact_token_manager.h>
#include <auth/turntoken_generator.h>
#include <dispatcher/dispatch_manager.h>
#include <config/challenge_config.h>
//#include <limiters/bcm_limiters.h>

namespace bcm {

class AccountsController : public std::enable_shared_from_this<AccountsController>
                         , public HttpRouter::Controller {
public:
    AccountsController(std::shared_ptr<AccountsManager> accountsManager,
                       std::shared_ptr<dao::SignUpChallenges> challenges,
                       std::shared_ptr<TurnTokenGenerator> turnTokenGenerator,
                       std::shared_ptr<DispatchManager> dispatchManager,
                       std::shared_ptr<KeysManager> keysManager,
                       ChallengeConfig challengeConfig);

    ~AccountsController() = default;

    void addRoutes(HttpRouter& router) override;

public:
    void challenge(HttpContext& context);
    void signup(HttpContext& context);
    void signin(HttpContext& context);
    void destroy(HttpContext& context);

    void setAttributes(HttpContext& context);
    void registerApn(HttpContext& context);
    void unregisterApn(HttpContext& context);
    void registerGcm(HttpContext& context);
    void unregisterGcm(HttpContext& context);
    void setFeatures(HttpContext& context);

    void getTurnToken(HttpContext& context);

private:
    void clearRedisDbUserPushInfo(const std::string& uid);

private:
    std::shared_ptr<AccountsManager> m_accountsManager;
    std::shared_ptr<dao::SignUpChallenges> m_challenges;
    std::shared_ptr<dao::GroupUsers> m_groupUsers;
    std::shared_ptr<TurnTokenGenerator> m_turnTokenGenerator;
    std::shared_ptr<DispatchManager> m_dispatchManager;
    std::shared_ptr<KeysManager> m_keysManager;
    ChallengeConfig m_challengeConfig;
};

}
