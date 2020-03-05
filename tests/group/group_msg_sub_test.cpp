#include "../test_common.h"

#include "group/group_msg_sub.h"
#include "utils/libevent_utils.h"
#include "redis/async_conn.h"
#include "redis/reply.h"

#include <hiredis/hiredis.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <boost/core/ignore_unused.hpp>

static const std::string kChan = "group_11";
static const std::vector<std::string> kMsgList = {"hello", "shutdown"};

class MessageHandler : public bcm::GroupMsgSub::IMessageHandler {
    bcm::GroupMsgSub& m_sub;
    std::size_t m_msgIdx;

public:
    MessageHandler(bcm::GroupMsgSub& sub)
        : m_sub(sub), m_msgIdx(0)
    {
    }

    void handleMessage(const std::string& chan, 
                        const std::string& msg) override
    {
        REQUIRE(chan == kChan);
        REQUIRE(msg == kMsgList[m_msgIdx % kMsgList.size()]);

        if (m_msgIdx++ == (kMsgList.size() - 1)) {
            std::vector<uint64_t> v;
            v.push_back(11);
            m_sub.unsubcribeGids(v);

            TLOG << "shutdown group message subscriber";
            // m_sub.shutdown([](int status) {
            //     boost::ignore_unused(status);
            //     TLOG << "group message subscriber is shutdown";
            // });

        }
    }
};

class Publisher : bcm::libevent::AsyncTask {
    struct event_base* m_eb;
    bcm::redis::AsyncConn m_conn;
    std::size_t m_msgIdx;

public:
    Publisher(struct event_base* eb, const std::string& host,
              int port = 6379,
              const std::string& password = "")
        : bcm::libevent::AsyncTask(eb), m_eb(eb)
        , m_conn(eb, host, port, password)
        , m_msgIdx(0)
    {
        m_conn.start(std::bind(&Publisher::onConnect, this, 
                                std::placeholders::_1));
    }

private:
    void onConnect(int status)
    {
        boost::ignore_unused(status);
        TLOG << "Publisher redis connected";
        executeWithDelay(200);
    }

    void run() override
    {
        TLOG << "Publisher run  chan: " << kChan << " msgid: " << m_msgIdx;
        m_conn.exec([this](int res, const bcm::redis::Reply& reply) {
                        REQUIRE(REDIS_OK == res);
                        REQUIRE(reply.isInteger());
                        TLOG << "Publisher redis: " << reply.getInteger();

                        if (++m_msgIdx < kMsgList.size()) {
                            executeWithDelay(200);
                            return;
                        }

                        m_conn.shutdown([](int status) {
                            boost::ignore_unused(status);
                            TLOG << "publisher shutdown";
                        });

                    }, "PUBLISH %b %b", kChan.c_str(), kChan.size(),
                    kMsgList[m_msgIdx].c_str(), kMsgList[m_msgIdx % kMsgList.size()].size());
    }

};

TEST_CASE("GroupMsgSub")
{
    evthread_use_pthreads();
    struct event_base* eb = event_base_new();

    // to use onlineRedisManager instance
    bcm::GroupMsgSub sub;

    MessageHandler msgHandler(sub);
    sub.addMessageHandler(&msgHandler);

    sleep(1);
    std::vector<uint64_t> v;
    v.push_back(11);
    sub.subscribeGids(v);

    std::thread t([eb]() {
        int res = 0;
        try {
            res = event_base_dispatch(eb);
        } catch (std::exception& e) {
            TLOG << "Publisher exception  res: " << res << " what: " << e.what();
        }

        TLOG << "event loop thread exit with code: " << res;
    });
    sleep(1);
    Publisher pub(eb, "127.0.0.1");

    t.join();
    TLOG << "thread terminated";

    event_base_free(eb);
}