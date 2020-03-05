#include "../test_common.h"
#include "redis/async_conn.h"
#include "redis/reply.h"

#include <event2/event.h>
#include <event2/thread.h>
#include <hiredis/hiredis.h>
#include <boost/core/ignore_unused.hpp>

#include <thread>

class RedisClient
{
    struct event_base* m_eb;
    bcm::redis::AsyncConn m_conn;

public:
    RedisClient(struct event_base* eb, const std::string& host, 
                int port = 6379, const std::string& password = "")
        : m_eb(eb), m_conn(eb, host, port, password)
    {
        m_conn.start(std::bind(&RedisClient::onConnect, this, 
                                std::placeholders::_1));
    }

private:
    void onConnect(int status)
    {
        boost::ignore_unused(status);
        m_conn.exec(std::bind(&RedisClient::onSetReply, this, 
                                std::placeholders::_1, std::placeholders::_2), 
                    "SET foo bar");
    }

    void onSetReply(int res, const bcm::redis::Reply& reply)
    {
        REQUIRE(REDIS_OK == res);
        REQUIRE(reply.isStatus());
        REQUIRE(reply.getStatus() == "OK");

        m_conn.shutdown([](int status) {
            boost::ignore_unused(status);
            TLOG << "client shutdown";
        });
    }
};

TEST_CASE("Cmd")
{
    evthread_use_pthreads();
    struct event_base* eb = event_base_new();
    std::thread t([eb]() {
        int res = event_base_dispatch(eb);
        TLOG << "event loop thread exit with code: " << res;
    });

    RedisClient cli(eb, "127.0.0.1");

    t.join();
    event_base_free(eb);
}