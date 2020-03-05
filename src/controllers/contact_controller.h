#pragma once

#include <http/http_router.h>
#include <dao/contacts.h>
#include <store/contact_token_manager.h>
#include <store/accounts_manager.h>
#include <dispatcher/dispatch_manager.h>
#include "config/size_check_config.h"

namespace bcm {

class ContactController : public std::enable_shared_from_this<ContactController>
                        , public HttpRouter::Controller {
public:
    ContactController(std::shared_ptr<dao::Contacts> contacts
                      , std::shared_ptr<AccountsManager> accountsManager
                      , std::shared_ptr<DispatchManager> dispatchMgr
                      , std::shared_ptr<OfflineDispatcher> offlineDispatcher
                      , const SizeCheckConfig& cfg);
    ~ContactController() = default;

    void addRoutes(HttpRouter& router) override;

private:
    // new
    void storeInParts(HttpContext& context);
    void restoreInParts(HttpContext& context);

    // contacts bloom filters
    void putContactsFilters(HttpContext& context);
    void patchContactsFilters(HttpContext& context);
    void deleteContactsFilters(HttpContext& context);

    // friend
    void friendRequest(HttpContext& context);
    void friendReply(HttpContext& context);
    void deleteFriend(HttpContext& context);

    uint32_t getValidAccount(const std::string& uid, Account& account);
    std::string encryptUidWithAccount(const std::string& uid, 
                                      const Account& account);

private:
    std::shared_ptr<dao::Contacts> m_contacts;
    std::shared_ptr<AccountsManager> m_accounts;
    std::shared_ptr<DispatchManager> m_dispatchMgr;
    std::shared_ptr<OfflineDispatcher> m_offlineDispatcher;
    SizeCheckConfig m_sizeCheckConfig;
};

}

