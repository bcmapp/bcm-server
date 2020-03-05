#include "../test_common.h"
#include "redis/async_conn.h"
#include "redis/reply.h"
#include "utils/libevent_utils.h"

#include <event2/event.h>
#include <event2/thread.h>
#include <hiredis/hiredis.h>
#include <boost/core/ignore_unused.hpp>

#include <thread>

static const std::string kChan = "__sub_pub_test_chan__";
static const std::string kChn2 = "test_channel2";
static const std::string kChn3 = "test_channel3";

static const std::vector<std::string> kMsgList = {"eye", "have", "you"};

static const std::string g_pSubStr = kChan;  // "__sub_pub_*" ;


class Subscriber : public bcm::redis::AsyncConn::ISubscriptionHandler {
    struct event_base* m_eb;
    bcm::redis::AsyncConn m_conn;
    std::size_t m_msgIdx;
    std::size_t m_kChan1, m_kChan2, m_kChan3;

public:
    Subscriber(struct event_base* eb, const std::string& host, int port = 6379, 
               const std::string& password = "")
        : m_eb(eb), m_conn(eb, host, port, password), m_msgIdx(0), m_kChan1(0), m_kChan2(0), m_kChan3(0)
    {
        TLOG << "subscriber init" ;
        m_conn.start(std::bind(&Subscriber::onConnect, this, 
                                std::placeholders::_1));
    }

private:
    void onConnect(int status)
    {
        boost::ignore_unused(status);
        TLOG << "subscriber redis connected  vector";
        m_conn.psubscribe(g_pSubStr, this);

        std::vector<std::string> v;
        v.push_back(kChn2);
        v.push_back(kChn3);

        m_conn.psubscribeBatch(v, this);
    }

    void onSubscribe(const std::string& chan) override
    {
        TLOG << "subscriber channel: " << chan << " subscribed";
        if( chan == g_pSubStr || chan == kChn2 || chan == kChn3 ) {

        } else {
            REQUIRE(chan == (g_pSubStr));
        }
    }
    
    void onUnsubscribe(const std::string& chan) override
    {
        TLOG << "subscriber channel: " << chan << " unsubscribed";
        if (chan == g_pSubStr || chan == kChn2 || chan == kChn3) {

        } else {
            REQUIRE(chan == (g_pSubStr));
        }

        if (chan == kChan) {
            REQUIRE(3 == m_kChan1);
        }
        if (chan == kChn2) {
            REQUIRE(3 == m_kChan2);
        }
        if (chan == kChn3) {
            REQUIRE(3 == m_kChan3);

            m_conn.shutdown([](int status) {
                boost::ignore_unused(status);
                TLOG << "subscriber shutdown";
            });
        }

    }
    
    void onMessage(const std::string& chan, const std::string& msg)
    {
        TLOG << "subscriber mesasge received: " << msg << " from channel: " << chan;
        if (chan == kChan) {
            m_kChan1++;
        } else if (chan == kChn2) {
            m_kChan2++;
        } else if (chan == kChn3) {
            m_kChan3++;
        } else {
            REQUIRE(chan == kChan);
        }

        REQUIRE(msg == kMsgList[m_msgIdx % 3 ]);
        ++m_msgIdx;
        if (m_msgIdx >= (kMsgList.size() * 2) ) {
            TLOG << chan << " subscription message received";

            if (m_msgIdx == (kMsgList.size() * 2)) {
                REQUIRE(m_conn.isSubcribeChannel(kChn2) == true);
                REQUIRE(m_conn.isSubcribeChannel(g_pSubStr) == true);

                std::vector<std::string> v;
                v.push_back(kChn2);
                v.push_back(g_pSubStr);
                m_conn.punsubscribeBatch(v);
            }
        }

        if (m_msgIdx >= (kMsgList.size() * 3 ) ) {
            TLOG << chan << " subscription message received";

            REQUIRE(m_conn.isSubcribeChannel(kChn2) == false);
            REQUIRE(m_conn.isSubcribeChannel(g_pSubStr) == false);
            REQUIRE(m_conn.isSubcribeChannel(kChn3) == true);

            m_conn.punsubscribe(kChn3);
        }

    }
    
    void onError(int code)
    {
        TLOG << "subscriber redis error: " << code;
        FAIL();
        m_conn.shutdown([](int status) {
            boost::ignore_unused(status);
            TLOG << "subscriber shutdown";
        });
    }
};

class Publisher : bcm::libevent::AsyncTask {
    struct event_base* m_eb;
    bcm::redis::AsyncConn m_conn;
    std::size_t m_msgIdx;

public:
    Publisher(struct event_base* eb, const std::string& host, int port = 6379, 
              const std::string& password = "")
        : bcm::libevent::AsyncTask(eb), m_eb(eb)
        , m_conn(eb, host, port, password)
        , m_msgIdx(0)
    {
        TLOG << "Publisher init";
        m_conn.start(std::bind(&Publisher::onConnect, this, 
                                std::placeholders::_1));
    }

private:
    void onConnect(int status)
    {
        boost::ignore_unused(status);
        TLOG << "redis Publisher connected";
        executeWithDelay(200);
    }

    void run() override
    {
        std::string tmpChan;
        if (m_msgIdx < kMsgList.size()) {
            tmpChan = kChan;
        } else {
            if (m_msgIdx < kMsgList.size() * 2) {
                tmpChan = kChn2;
            } else {
                tmpChan = kChn3;
            }
        }

        m_conn.exec([this](int res, const bcm::redis::Reply& reply) {
                        REQUIRE(REDIS_OK == res);
                        REQUIRE(reply.isInteger());
                        TLOG << "redis: " << reply.getInteger();

                        if (m_msgIdx < kMsgList.size() * 3) {
                            executeWithDelay(200);
                            return;
                        }

                        m_conn.shutdown([](int status) {
                            boost::ignore_unused(status);
                            TLOG << "publisher shutdown";
                        });
                    }, "PUBLISH %b %b", tmpChan.c_str(), tmpChan.size(),
                    kMsgList[m_msgIdx % 3 ].c_str(), kMsgList[m_msgIdx % 3].size());

        ++m_msgIdx;
    }
};

TEST_CASE("SubPub")
{
    evthread_use_pthreads();
    struct event_base* eb = event_base_new();

    Subscriber sub(eb, "127.0.0.1");
    Publisher pub(eb, "127.0.0.1");

    std::thread t([eb]() {
        int res = event_base_dispatch(eb);

        TLOG << "event loop thread exit with code: " << res;
    });

    t.join();

    TLOG << "thread terminated";

    event_base_free(eb);
}