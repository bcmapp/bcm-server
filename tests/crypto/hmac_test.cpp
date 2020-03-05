#include "../test_common.h"

#include <crypto/hmac.h>
#include <crypto/hex_encoder.h>

using namespace bcm;

TEST_CASE("Hmac")
{
    REQUIRE(HexEncoder::encode(Hmac::digest(Hmac::Algo::SHA1, "test", "test")) == "0c94515c15e5095b8a87a50ba0df3bf38ed05fe6");

    Hmac hmac(Hmac::Algo::SHA256, "testhmac");
    hmac.update("0c94515c15e5095b8a87a50ba0df3bf38ed05fe6");
    REQUIRE(HexEncoder::encode(hmac.final()) == "0b584da8fb829bffeca4541ce13a986c293b74cb7dee662cb9fc2013d9d7c3de");

    hmac.reset(Hmac::Algo::SHA512, "HmacSha512");
    hmac.update("0b584da8fb829bffeca4541ce13a986c293b74cb7dee662cb9fc2013d9d7c3de");
    REQUIRE(hmac.final() == HexEncoder::decode("f70d806dc29280ae5f7990eed603446005c3de7690c9341bc5a10a33761ffcdb317580eba84220fb325691392b429da79528588212c6fe9965903b52cbe908e5"));

}
