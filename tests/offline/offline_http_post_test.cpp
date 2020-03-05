#include "../test_common.h"
#include <unistd.h>
#include "unistd.h"

#include <thread>
#include <mutex>

#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/thread/barrier.hpp>
#include <fiber/asio_yield.h>

#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/thread.h>

#include <nlohmann/json.hpp>
#include <thread>
#include <shared_mutex>

#include <utils/jsonable.h>
#include <utils/time.h>
#include <utils/log.h>
#include "utils/libevent_utils.h"
#include "utils/thread_utils.h"

#include "group/io_ctx_executor.h"
#include "utils/sync_latch.h"

#include "../../src/program/offline-server/http_post_request.h"
#include "../../src/program/offline-server/offline_server_entities.h"
#include "../../src/config/group_store_format.h"

static const int kPoolSize = 5;

using namespace bcm;

class OfflinePushServiceImpl
{
    IoCtxExecutor m_workThdPool;
    
    boost::asio::ssl::context m_sslCtx;

public:
    OfflinePushServiceImpl()
            : m_workThdPool(3)
            , m_sslCtx(ssl::context::sslv23)
    {
    }
    
    virtual ~OfflinePushServiceImpl()
    {

    }
    
    void doPostGroupMessage(const std::string& imSvrAddr,
                          uint64_t gid, uint64_t mid,
                          const std::map<std::string, std::string>& destinations)
    {
        nlohmann::json j = {
                {"gid", std::to_string(gid)},
                {"mid", std::to_string(mid)},
                {"destinations", destinations}
        };
    
        TLOG << "offline server: " << imSvrAddr << " are currently online";
        
        HttpPostRequest::shared_ptr req =
                std::make_shared<HttpPostRequest>(*(m_workThdPool.io_context()), m_sslCtx, kOfflinePushMessageUrl);
        req->setServerAddr(imSvrAddr)->setPostData(j.dump())->exec();
    }
};


TEST_CASE("http_post_push")
{

    evthread_use_pthreads();
    
    struct event_base* eb = event_base_new();
    std::unique_ptr<std::thread> m_thread;

    
    OfflinePushServiceImpl  pushSvr;
    
    std::string  targetIp   = "127.0.0.1:8080";
    
    std::map<std::string /* uid */, std::string>  offlineUids;
    GroupUserMessageIdInfo  tmp1, tmp2, tmp3;
    tmp1.last_mid = 111;
    tmp1.gcmId = "33333";
    offlineUids["ddddddddd"] = tmp1.to_string();
    tmp2.last_mid = 111;
    tmp2.umengId  = "44444";
    tmp2.osType   = 12;
    tmp2.bcmBuildCode = 1024;
    offlineUids["eeeeeeeeeeeee"] = tmp2.to_string();
    tmp3.last_mid = 111;
    tmp3.apnId = "55555555";
    tmp3.apnType = "eee";
    tmp3.voipApnId = "fff";
    offlineUids["ffffffff"] = tmp3.to_string();
    
    pushSvr.doPostGroupMessage(targetIp, 1, 111, offlineUids);

    std::thread t([eb]() {
        int res = 0;
        try {
            res = event_base_dispatch(eb);
            sleep(5);
        } catch (std::exception& e) {
            TLOG << "Publisher exception  res: " << res << " what: " << e.what();
        }
        TLOG << "event loop thread exit with code: " << res;
    });
    t.join();
    
    event_base_free(eb);

}