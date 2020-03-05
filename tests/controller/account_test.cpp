#include "../test_common.h"

#include <controllers/account_entities.h>
#include <utils/json_serializer.h>

using namespace bcm;

TEST_CASE("Account")
{
    AccountAttributesSigned attr;
    std::unique_ptr<JsonSerializer> serializer = std::make_unique<JsonSerializerImp<AccountAttributesSigned>>();

    std::string result;
    attr.nonce = 0xffffffff;
    bool ret = serializer->serialize(attr, result);
    TLOG << result << ret;

    boost::any attr2;
    ret = serializer->deserialize(result, attr2);
    TLOG << ret;

}
