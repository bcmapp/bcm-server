#include "im_server_mgr.h"
#include "redis/reply.h"
#include "config/redis_config.h"
#include "registers/imservice_register.h"
#include "utils/log.h"

#include <event2/event.h>
#include <hiredis/hiredis.h>

#include <sstream>

namespace bcm {

static const int kTickIntervalSecs = 5;

static void handleTick(evutil_socket_t fd, short events, void* arg)
{
    boost::ignore_unused(fd, events);
    ImServerMgr* pThis = reinterpret_cast<ImServerMgr*>(arg);
    pThis->onTick();
}

ImServerMgr::ImServerMgr(struct event_base* eb, const RedisConfig& redisCfg)
    : m_redisConn(eb, redisCfg.ip, redisCfg.port, redisCfg.password)
    , m_listener(nullptr)
{
    m_redisConn.start(std::bind(&ImServerMgr::onConnect, this, 
                                std::placeholders::_1));

    srand(time(NULL));
    event_assign(&m_evtTick, eb, -1, EV_READ | EV_PERSIST, handleTick, 
                 reinterpret_cast<void*>(this));
    struct timeval tv;
    evutil_timerclear(&tv);
    tv.tv_sec = kTickIntervalSecs;
    event_add(&m_evtTick, &tv);
}

void ImServerMgr::onConnect(int status)
{
    boost::ignore_unused(status);
}

void ImServerMgr::onTick()
{
    pingRedis();
    updateImServerList();
}

void ImServerMgr::addRegKey(const std::string& key)
{
    if (std::find(m_regKeys.begin(), m_regKeys.end(), key) == m_regKeys.end()) {
        m_regKeys.emplace_back(key);
    }
}

void ImServerMgr::shutdown(DisconnectHandler&& handler)
{
    event_del(&m_evtTick);
    m_redisConn.shutdown(std::forward<DisconnectHandler>(handler));
}

bool ImServerMgr::shouldHandleGroup(uint64_t gid)
{
    std::shared_lock<std::shared_timed_mutex> l(m_constHashMtx);
    std::string addr = m_consistentHash.GetServer(gid);
    if (addr.empty()) {
        LOGW << "could not find im server by group id; " << gid;
        return false;
    }
    if (!isSelf(addr)) {
        LOGW << "group, gid: " << gid << " does not belong to me";
        return false;
    }
    return true;
}

std::string ImServerMgr::getServerByGroup(uint64_t gid)
{
    std::shared_lock<std::shared_timed_mutex> l(m_constHashMtx);
    return m_consistentHash.GetServer(gid);
}

std::string ImServerMgr::getServerRandomly() const
{
    static const std::string kEmptyStr;
    std::shared_lock<std::shared_timed_mutex> l(m_imsvrsMtx);
    if (m_imsvrs.empty()) {
        return kEmptyStr;
    }
    return m_imsvrs[rand() % m_imsvrs.size()];
}

bool ImServerMgr::isSelf(const std::string& addr) const
{
    for (const std::string& key : m_regKeys) {
        if (key.find(addr) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void ImServerMgr::setServerListUpdateListener(IServerListUpdateListener* l)
{
    m_listener = l;
}

void ImServerMgr::pingRedis()
{
    m_redisConn.exec([](int res, const redis::Reply& reply) {
        if (REDIS_OK != res) {
            LOGE << "send 'PING' command error: " << res;
        } else if (reply.isError()) {
            LOGE << "PING redis error: " << reply.getError();
        } else if (reply.isStatus()) {
            if ( (rand() % 10) == 0 ) {
                LOGI << "PING redis: " << reply.getString();;
            }
        } else {
            LOGE << "PING redis error: unexpected reply type received: " 
                 << reply.type();
        }
    }, "PING");
}

void ImServerMgr::updateImServerList()
{
    LOGD << "update server list";
    m_redisConn.exec([this](int res, const redis::Reply& reply) {
        if (REDIS_OK != res) {
            LOGE << "send 'PUBSUB CHANNELS imserver_*' command error: " << res;
            return;
        }
        if (reply.isError()) {
            LOGE << "get im server list from redis error: " 
                 << reply.getError();
            return;
        }
        if (!reply.isArray()) {
            LOGE << "get im server list from redis error: "
                    "unexpected reply type received: " << reply.type();
            return;
        }
        std::vector<std::string> serverDescList = reply.getStringList();
        if (serverDescList.empty()) {
            LOGI << "im server list is empty";
            return;
        }

        LOGD << "found " << serverDescList.size() << " im server(s)";

        bool shouldLog = ((rand() % 10) == 0);
        std::unique_lock<std::shared_timed_mutex> l(m_imsvrsMtx);
        std::unique_lock<std::shared_timed_mutex> ll(m_constHashMtx);
        m_imsvrs.clear();
        m_consistentHash.Clear();
        
        m_imsvrs.reserve(serverDescList.size());
        for (auto& ipPortEnt : serverDescList) {
            std::vector<std::string> tokens;
            boost::split(tokens, ipPortEnt, boost::is_any_of("_"));
            if (tokens.size() >= 2) {
                std::string& ipport = tokens[1];
                if (std::find(m_imsvrs.begin(), m_imsvrs.end(), ipport) 
                        == m_imsvrs.end()) {
                    m_imsvrs.emplace_back(ipport);
                    m_consistentHash.AddServer(ipport);
                }
            } else {
                LOGE << "invalid server descriptor format: " << ipPortEnt;
            }
        }
        if (shouldLog) {
            std::stringstream ss;
            for (auto& ent : m_imsvrs) {
                if (ss.tellp() > 0) {
                    ss << ", ";
                }
                ss << ent;
            }
            LOGI << "found " << m_imsvrs.size() << " im servers: " << ss.str();
        }

        ll.unlock();
        l.unlock();
        
        if (m_listener != nullptr) {
            m_listener->onServerListUpdate();
        }

    }, "PUBSUB CHANNELS imserver_*");
}

} // namespace bcm