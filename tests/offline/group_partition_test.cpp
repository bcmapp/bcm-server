#include "../test_common.h"

#include <event2/event.h>
#include <event2/thread.h>
#include <hiredis/hiredis.h>
#include <boost/core/ignore_unused.hpp>

#include <utils/jsonable.h>


#include "../../src/program/offline-server/group_partition_mgr.h"
#include "../../src/config/group_store_format.h"
#include <thread>


using namespace bcm;

bool requireGUM(GroupUserMessageIdInfo a, GroupUserMessageIdInfo b)
{
    if (a.last_mid != b.last_mid) return false;
    if (a.gcmId != b.gcmId) return false;
    if (a.umengId != b.umengId) return false;
    if (a.osType != b.osType) return false;
    if (a.bcmBuildCode != b.bcmBuildCode) return false;
    if (a.apnId != b.apnId) return false;
    if (a.apnType != b.apnType) return false;
    if (a.voipApnId != b.voipApnId) return false;
    
    return true;
}

TEST_CASE("group_partition_test")
{
    GroupUserMessageIdInfo  gum, gum2, gum3, gum4;
    gum.last_mid   = 1111111;

    std::string str2 = gum.to_string();
    TLOG << "group user message:  only last_mid: " << str2;
    gum2.from_string(str2);
    REQUIRE(requireGUM(gum2, gum) == true);
    
    gum.gcmId = "erqrtr";

    str2 = gum.to_string();
    TLOG << "group user message: gcmid: " << str2;
    gum2.from_string(str2);
    REQUIRE(requireGUM(gum2, gum) == true);

    gum.gcmId = "";
    gum.umengId = "ereqtwert";
    gum.osType = 33;
    gum.bcmBuildCode = 3241;

    str2 = gum.to_string();
    TLOG << "group user message: umengId: " << str2;
    gum3.from_string(str2);
    REQUIRE(requireGUM(gum3, gum) == true);

    gum.umengId = "";
    gum.apnId = "wetrew";
    gum.apnType = "fregrew";
    gum.voipApnId = "ewqte";

    str2 = gum.to_string();
    TLOG << "group user message: apnid: " << str2;
    gum4.from_string(str2);
    gum.osType = 0;
    gum.bcmBuildCode = 0;
    REQUIRE(requireGUM(gum4, gum) == true);
    
    //
    GroupPartitionMgr::Instance().updateMid(111, 1111, 2222);

    GroupMessageSeqInfo  gmS;
    REQUIRE(GroupPartitionMgr::Instance().getGroupPushInfo(111, gmS) == true);
    REQUIRE(gmS.timestamp == 1111);
    REQUIRE(gmS.lastMid == 2222);

    REQUIRE(GroupPartitionMgr::Instance().getGroupPushInfo(112, gmS) == false);
    
    GroupPartitionMgr::Instance().updateMid(111, 1112, 2223);
    GroupPartitionMgr::Instance().updateMid(112, 1113, 2224);

    REQUIRE(GroupPartitionMgr::Instance().getGroupPushInfo(111, gmS) == true);
    REQUIRE(gmS.timestamp == 1112);
    REQUIRE(gmS.lastMid == 2223);

    REQUIRE(GroupPartitionMgr::Instance().getGroupPushInfo(112, gmS) == true);
    REQUIRE(gmS.timestamp == 1113);
    REQUIRE(gmS.lastMid == 2224);

}