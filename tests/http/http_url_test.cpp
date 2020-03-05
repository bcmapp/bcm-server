#include "../test_common.h"
#include <http/http_url.h>

using namespace bcm;

TEST_CASE("UrlParser")
{
    {
        HttpUrl url("http://google.com:1080");
        REQUIRE(!url.invalid());
        REQUIRE(url.protocol() == "http");
        REQUIRE(url.host() == "google.com");
        REQUIRE(!url.isIPv6());
        REQUIRE(url.port() == 1080);
        REQUIRE(url.query().empty());
    }

    {
        HttpUrl url("https://google.com");
        REQUIRE(!url.invalid());
        REQUIRE(url.protocol() == "https");
        REQUIRE(url.host() == "google.com");
        REQUIRE(!url.isIPv6());
        REQUIRE(url.port() == 443);
        REQUIRE(url.query().empty());
    }

    {
        HttpUrl url("http://google.com?xxx=xxx");
        REQUIRE(!url.invalid());
        REQUIRE(url.protocol() == "http");
        REQUIRE(url.host() == "google.com");
        REQUIRE(!url.isIPv6());
        REQUIRE(url.port() == 80);
        REQUIRE(url.query() == "xxx=xxx");
    }

    {
        HttpUrl url("http://[::]");
        REQUIRE(!url.invalid());
        REQUIRE(url.protocol() == "http");
        REQUIRE(url.host() == "::");
        REQUIRE(url.isIPv6());
        REQUIRE(url.port() == 80);
        REQUIRE(url.query().empty());
    }

    {
        HttpUrl url("https://[::]:1080");
        REQUIRE(!url.invalid());
        REQUIRE(url.protocol() == "https");
        REQUIRE(url.host() == "::");
        REQUIRE(url.isIPv6());
        REQUIRE(url.port() == 1080);
        REQUIRE(url.query().empty());
    }

    {
        HttpUrl url("https://[::]:1080?xxx=xxx");
        REQUIRE(!url.invalid());
        REQUIRE(url.protocol() == "https");
        REQUIRE(url.host() == "::");
        REQUIRE(url.isIPv6());
        REQUIRE(url.port() == 1080);
        REQUIRE(url.query() == "xxx=xxx");
    }

    {
        REQUIRE(HttpUrl("https://127.0.0.1:108a?xxx=xxx").invalid());
        REQUIRE(HttpUrl("https:/127.0.0.1").invalid());
        REQUIRE(HttpUrl("https://[::x:1080?xxx=xxx").invalid());
    }
}

