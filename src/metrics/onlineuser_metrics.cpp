#include "onlineuser_metrics.h"
#include "metrics_client.h"
#include "dispatcher/dispatch_manager.h"
#include <thread>
#include <chrono>

namespace bcm
{

using namespace metrics;

OnlineUserMetrics::OnlineUserMetrics(std::shared_ptr <DispatchManager> dispatchManager,
                                     int64_t reportIntervalInMs)
        : m_dispatchManager(std::move(dispatchManager))
        , m_reportIntervalInMs(reportIntervalInMs)
{
}

void OnlineUserMetrics::start()
{
    m_reportThread = std::thread([&](){
        std::string threadName = "m.online_user";
        setCurrentThreadName(threadName);
        LOGI << threadName << " started!";
        
        while(true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(m_reportIntervalInMs));

            MetricsClient::Instance()->counterSet("o_onlineuser", m_dispatchManager->getDispatchCount());
        }

    });
    m_reportThread.detach();

}

}
