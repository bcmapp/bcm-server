#include "../test_common.h"

#include "bloom/bloom_filters.h"
#include <vector>
#include <string>

using namespace bcm;

TEST_CASE("BloomFiltersTest")
{
    BloomFilters filters1(0,1024);
    REQUIRE(filters1.contains("aaa") == false);
    filters1.insert("aaa");
    REQUIRE(filters1.contains("aaa") == true);
    REQUIRE(filters1.contains("bbb") == false);

    BloomFilters filters2(1,128);
    REQUIRE(filters2.contains("aaa") == false);
    REQUIRE(filters2.contains("bbb") == false);
    filters2.insert("ccc");
    REQUIRE(filters2.contains("ccc") == false);
    REQUIRE(filters2.contains("aaa") == false);

    BloomFilters filters3(0,128);
    REQUIRE(filters3.contains("aaa") == false);
    REQUIRE(filters3.contains("bbb") == false);
    filters3.insert("ccc");
    REQUIRE(filters3.contains("aaa") == false);
    REQUIRE(filters3.contains("bbb") == false);
    REQUIRE(filters3.contains("ccc") == true);
}
