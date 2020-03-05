#include "../test_common.h"
#include <http/http_client.h>
#include <fiber/fiber_pool.h>
#include <utils/sync_latch.h>

using namespace bcm;

TEST_CASE("HttpClient")
{
    FiberPool pool(1);
    pool.run();

    pool.post(pool.getIOContext(), [](){
        auto get = HttpGet("http://www.nossl.net/");
        REQUIRE(get.process(*FiberPool::getThreadIOContext()));
        TLOG << get.response();
    });

    pool.post(pool.getIOContext(), [](){
        auto get = HttpGet("https://157.255.229.119:8080/v1/accounts/challenge/1971vifrJ1KkL3JKAU8rLSDEN9aJt5HJnx");
        REQUIRE(get.process(*FiberPool::getThreadIOContext()));
        TLOG << get.response();
        REQUIRE(get.response().result() == http::status::ok);
    });

    pool.post(pool.getIOContext(), [](){
        auto put = HttpPut("https://157.255.229.119:8080/v1/accounts/signup");
        put.body("application/json", "{}");
        REQUIRE(put.process(*FiberPool::getThreadIOContext()));
        TLOG << put.response();
        REQUIRE(put.response().result() == http::status::bad_request);
    });

    sleep(5);
    pool.stop();
}

