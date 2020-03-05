#include <cinttypes>
#include "../test_common.h"
#include "../../src/dao/dao_cache/fifo_cache.h"
#include "../../src/proto/dao/group_keys.pb.h"
#include "../../src/controllers/group_manager_entities.h"

void compareKey(const bcm::GroupKeys& lhs, const bcm::GroupKeys& rhs)
{
    REQUIRE(lhs.gid() == rhs.gid());
    REQUIRE(lhs.version() == rhs.version());
    REQUIRE(lhs.groupowner() == rhs.groupowner());
    REQUIRE(lhs.groupkeys() == rhs.groupkeys());
    REQUIRE(lhs.mode() == rhs.mode());
    REQUIRE(lhs.creator() == rhs.creator());
    REQUIRE(lhs.createtime() == rhs.createtime());
}

TEST_CASE("FIFOCache")
{
    bcm::dao::FIFOCache<std::string, bcm::GroupKeys> cache(1024 * 128);

    for (int i = 0; i < 128; i++) {
        bcm::GroupKeysJson json;
        json.encryptVersion = 1;
        for (int j = 0; j < 128; j++) {
            bcm::GroupKeyEntryV0 entry;
            entry.uid = "member" + std::to_string(j);
            entry.key = "key" + std::to_string(j);
            entry.deviceId = 0;
            json.keysV0.emplace_back(entry);
        }
        bcm::GroupKeys keys;
        keys.set_gid(i);
        keys.set_version(0);
        keys.set_groupowner("member" + std::to_string(i) + "0");
        keys.set_creator(keys.groupowner());
        keys.set_createtime(i);
        keys.set_groupkeys(nlohmann::json(json).dump());
        keys.set_mode(bcm::GroupKeys::ONE_FOR_EACH);

        std::string k = std::to_string(i) + "0";
        REQUIRE(cache.set(k, keys) == true);
        bcm::GroupKeys res;
        REQUIRE(cache.get(k, res) == true);
        compareKey(keys, res);
    }
}
