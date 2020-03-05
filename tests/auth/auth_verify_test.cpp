#include "../test_common.h"

#include "auth/authenticator.h"
#include "store/accounts_manager.h"

using namespace bcm;

TEST_CASE("AuthenticatorVerify")
{
    Authenticator::Credential credential;
    credential.salt = "1373176575";
    credential.token = "7cfe97d4a0f94dc6801415d07dbe194ee133e86b";
    std::string userToken = "password";
    bool authRes = Authenticator::verify(credential, userToken);
    REQUIRE(authRes);
    credential.salt = "111";
    authRes = Authenticator::verify(credential, userToken);
    REQUIRE(!authRes);
    credential.salt = "1373176575";
    authRes = Authenticator::verify(credential, "wrong password");
    REQUIRE(!authRes);
}
