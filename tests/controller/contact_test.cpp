#include "../test_common.h"

#include <controllers/contact_entities.h>
#include <utils/json_serializer.h>

using namespace bcm;

TEST_CASE("Contact")
{
    ContactResponse uids = ContactResponse();
    std::string result;
    std::unique_ptr<JsonSerializer> serializer = std::make_unique<JsonSerializerImp<ContactResponse>>();
    uids.contacts["token1"].push_back("uid1");
    uids.contacts["token1"].push_back("uid2");
    uids.contacts["token1"].push_back("uid3");
    (void)serializer->serialize(uids, result);
    TLOG << result;

    boost::any uids2;
    (void)serializer->deserialize(result, uids2);
    TLOG << boost::any_cast<ContactResponse>(uids2).contacts["token1"][1];

}
