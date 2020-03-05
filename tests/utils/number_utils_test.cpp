#include "../test_common.h"

#include <utils/number_utils.h>

using namespace bcm;

TEST_CASE("NumberUtils")
{
    auto hexEncode = [] (const std::string& data) {
        static constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                          '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
        std::string hexstr(data.size() * 2, ' ');
        for (size_t i = 0; i < data.size(); ++i) {
            hexstr[2 * i] = hexmap[(data[i] & 0xF0) >> 4];
            hexstr[2 * i + 1] = hexmap[data[i] & 0x0F];
        }
        return hexstr;
    };

    REQUIRE(hexEncode(NumberUtils::getNumberToken("+8615220261850")) == "02d9df30441a97b89aa6");

    REQUIRE(NumberUtils::isValidNumber("+85239235400"));
    REQUIRE(NumberUtils::isValidNumber("+8618080806060"));
    REQUIRE(NumberUtils::isValidNumber("+19377807552"));
    REQUIRE_FALSE(NumberUtils::isValidNumber("+618080806060"));
    REQUIRE_FALSE(NumberUtils::isValidNumber("+9377807552"));
}