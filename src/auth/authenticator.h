#pragma once
#include "store/accounts_manager.h"
#include "authorization_header.h"
#include <memory>
#include <string>
#include <boost/optional.hpp>

namespace bcm {

class Authenticator {
public:
    struct Credential {
        std::string token;
        std::string salt;
    };

    enum AuthResult {
        AUTHRESULT_SUCESS = 0,
        AUTHRESULT_ACCOUNT_NOT_FOUND = 1,
        AUTHRESULT_ACCOUNT_DELETED = 2,
        AUTHRESULT_DEVICE_NOT_FOUND = 3,
        AUTHRESULT_TOKEN_VERIFY_FAILED = 4,
        AUTHRESULT_DEVICE_ABNORMAL = 5,
        AUTHRESULT_AUTH_HEADER_ERROR = 6,
        AUTHRESULT_REQUESTID_NOT_FOUND = 7,
        AUTHRESULT_DEVICE_NOT_ALLOWED = 8,
        AUTHRESULT_UNKNOWN_ERROR = -1
    };

    enum AuthType {
        AUTHTYPE_NO_AUTH = 0,
        AUTHTYPE_ALLOW_ALL = 1,
        AUTHTYPE_ALLOW_MASTER = 2,
        AUTHTYPE_ALLOW_SLAVE = 3
    };

public:
    Authenticator();
    explicit Authenticator(std::shared_ptr<AccountsManager> accountsManager);
    virtual ~Authenticator() = default;

    AuthResult auth(const AuthorizationHeader& authHeader, const boost::optional<ClientVersion>& client,
                    Account& account, AuthType type = AUTHTYPE_ALLOW_ALL) const;

    void refreshAuthenticated(Account& account) const;

    static Credential getCredential(const std::string& userToken);

public:
    static bool verify(const Credential& credential, const std::string& userToken);

private:
    bool checkAndUpdateDevice(Account& account, const boost::optional<ClientVersion>& client) const;

private:
    std::shared_ptr<AccountsManager> m_accountsManager;
};

} // namespace bcm
