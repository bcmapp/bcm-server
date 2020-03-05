#include "../test_common.h"

#include "group/im_server_mgr.h"
#include "config/redis_config.h"
#include "redis/async_conn.h"
#include "redis/reply.h"
#include "utils/libevent_utils.h"

#include <event2/event.h>
#include <event2/thread.h>
#include <hiredis/hiredis.h>
#include <boost/core/ignore_unused.hpp>

#include <thread>

static const std::string kChan = "imserver_183.36.111.207:8080";
static const std::string kMsg = "shutdown";

class Subscriber : public bcm::redis::AsyncConn::ISubscriptionHandler {
    struct event_base* m_eb;
    bcm::redis::AsyncConn m_conn;

public:
    Subscriber(struct event_base* eb, const std::string& host, int port = 6379, 
               const std::string& password = "")
        : m_eb(eb), m_conn(eb, host, port, password)
    {
        m_conn.start(std::bind(&Subscriber::onConnect, this, 
                                std::placeholders::_1));
    }

private:
    void onConnect(int status)
    {
        boost::ignore_unused(status);
        TLOG << "redis connected";
        m_conn.subscribe(kChan, this);
    }

    void onSubscribe(const std::string& chan) override
    {
        TLOG << "channel: " << chan << " subscribed";
        REQUIRE(chan == kChan);
    }
    
    void onUnsubscribe(const std::string& chan) override
    {
        TLOG << "channel: " << chan << " unsubscribed";
        REQUIRE(chan == kChan);
        m_conn.shutdown([](int status) {
            boost::ignore_unused(status);
            TLOG << "subscriber shutdown";
        });
    }
    
    void onMessage(const std::string& chan, const std::string& msg)
    {
        TLOG << "mesasge received: " << msg << ", channel: " << chan;
        REQUIRE(chan == kChan);
        REQUIRE(msg == kMsg);
        m_conn.shutdown([](int status) {
            boost::ignore_unused(status);
            TLOG << "subscriber shutdown";
        });
    }
    
    void onError(int code)
    {
        TLOG << "redis error: " << code;
        FAIL();
        m_conn.shutdown([](int status) {
            boost::ignore_unused(status);
            TLOG << "subscriber shutdown";
        });
    }
};

class Checker : public bcm::libevent::AsyncTask {
    struct event_base* m_eb;
    bcm::ImServerMgr& m_imSvrMgr;
    bcm::redis::AsyncConn m_conn;

public:
    Checker(struct event_base* eb, bcm::ImServerMgr& imSvrMgr, 
            const std::string& host, int port = 6379, 
            const std::string& password = "")
        : bcm::libevent::AsyncTask(eb), m_eb(eb), m_imSvrMgr(imSvrMgr)
        , m_conn(eb, host, port, password)
    {
        srand(time(NULL));
        m_conn.start(std::bind(&Checker::onConnect, this, 
                                std::placeholders::_1));
    }

private:
    void onConnect(int status)
    {
        boost::ignore_unused(status);
        TLOG << "redis connected";
        executeWithDelay(1000);
    }

    void run() override
    {
        TLOG << "start check im server list";
        std::string addr = m_imSvrMgr.getServerRandomly();
        if (!addr.empty()) {
            TLOG << "found " << addr;

            REQUIRE( ("imserver_" + addr) == kChan );

            for (int i = 0; i < 100; i++) {
                uint64_t gid = rand();
                std::string addr = m_imSvrMgr.getServerByGroup(gid);
                if (addr.empty()) {
                    TLOG << "i=" << i << ", gid: " << gid;
                }
                REQUIRE_FALSE(addr.empty());
            }

            m_conn.exec([this](int res, const bcm::redis::Reply& reply) {
                REQUIRE(REDIS_OK == res);
                REQUIRE(reply.isInteger());
                TLOG << "redis: " << reply.getInteger();

                m_conn.shutdown([](int status) {
                    boost::ignore_unused(status);
                    TLOG << "checker is shutdown";
                });

                m_imSvrMgr.shutdown([](int status) {
                    boost::ignore_unused(status);
                    TLOG << "im server mgr is shutdown";
                });
            }, "PUBLISH %s %s", kChan.c_str(), kMsg.c_str());
        } else {
            TLOG << "im server not found";
            executeWithDelay(1000);
        }
    }
};

TEST_CASE("ImServerMgr")
{
    evthread_use_pthreads();
    struct event_base* eb = event_base_new();
    std::thread t([eb]() {
        int res = event_base_dispatch(eb);
        TLOG << "event loop thread exit with code: " << res;
    });

    bcm::RedisConfig redisCfg = {"127.0.0.1", 6379, "", ""};
    bcm::ImServerMgr imSvrMgr(eb, redisCfg);

    Subscriber sub(eb, "127.0.0.1");
    Checker chk(eb, imSvrMgr, "127.0.0.1");

    t.join();
    event_base_free(eb);
}


