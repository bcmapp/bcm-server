#include "../test_common.h"

#include "auth/authenticator.h"
#include "config/dao_config.h"
#include "store/accounts_manager.h"

using namespace bcm;


TEST_CASE("AuthenticatorAll")
{
    nlohmann::json j =  R"({
        "postgres" : {
            "host" : "47.106.219.70",
            "port" : 5432,
            "username" : "postgres",
            "password" : "amedata123",
            "read" : true,
            "write" : true
        },
        "pegasus" : {
            "config" : "../../resource/pegasus_config.ini",
            "cluster" : "bcm_kv_cluster",
            "read" : false,
            "write" : true
        },
        "timeout" : 3000
    })"_json;

    DaoConfig config = j;
    bool storeOk = dao::initialize(config);
    TLOG << "readRDB:" << config.postgres.read;
    TLOG << "readDKV:" << config.pegasus.read;
    TLOG << "writeRDB:" << config.postgres.write;
    TLOG << "writeDKV:" << config.pegasus.write;
    TLOG << "store init res:" << storeOk;
    auto accountsManager = std::make_shared<AccountsManager>();
    auto authenticator = std::make_shared<Authenticator>(accountsManager);
    std::shared_ptr<bcm::dao::Accounts> m_accounts = bcm::dao::ClientFactory::accounts();

    Account account;
    account.set_uid("haha_test");
    auto device = account.add_devices();
    device->set_id(2);
    device->set_salt("1373176575");
    device->set_authtoken("7cfe97d4a0f94dc6801415d07dbe194ee133e86b");
    auto updateRes = m_accounts->updateAccount(account, 0);

    TLOG << "updateAccountRes:" << updateRes;
    REQUIRE(updateRes);
    auto deviceRes = m_accounts->updateDevice(account, 2);
    TLOG << "updateDeviceRes:" << deviceRes;
    REQUIRE(deviceRes);
    AuthorizationHeader header{"haha_test", "password", 2};
    Account accountRes;
    auto authRes = authenticator->auth(header, boost::none, accountRes);
    auto dev = AccountsManager::getDevice(accountRes, 2);
    bool devRet = false;
    if (dev) {
        devRet = true;
    }
    REQUIRE(devRet);
    devRet = false;
    dev = AccountsManager::getAuthDevice(accountRes);
    if (dev) {
        devRet = true;
    }
    REQUIRE(devRet);
    REQUIRE(dev.get().id() == 2);
    REQUIRE(dev.get().salt() == "1373176575");
    REQUIRE(dev.get().authtoken() == "7cfe97d4a0f94dc6801415d07dbe194ee133e86b");
    REQUIRE(accountRes.uid() == "haha_test");
    TLOG << "authRes:" << authRes;
    REQUIRE(authRes == Authenticator::AUTHRESULT_SUCESS);
}
