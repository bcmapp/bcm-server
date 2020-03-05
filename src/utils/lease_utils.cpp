#include "lease_utils.h"
#include "dao/client.h"
#include "log.h"

#include <thread>
#include <chrono>

namespace bcm {

MasterLeaseAgent::MasterLeaseAgent(
    const std::string& key, 
    uint32_t ttlMs, 
    leaseLostCallback_t cb) 
    : m_leaseKey(std::move(key))
    , m_ttlMs(ttlMs)
    , m_masterLease(dao::ClientFactory::masterLease())
    , m_currentRole(ProcessHARole::Undefined)
    , m_callback(std::move(cb))
    , m_tid(0)
{
}

void MasterLeaseAgent::start()
{
    std::thread thd(&MasterLeaseAgent::loop, this);
    m_tid = thd.native_handle();
    thd.detach();
}

void MasterLeaseAgent::stop()
{
    // Release lease here
    m_masterLease->releaseLease(m_leaseKey);
    // TODO: pthread_kill first, if make no sense, cancel with pthread_cancel
    if (m_tid > 0) {
        pthread_cancel(m_tid);
    }
}

void MasterLeaseAgent::loop(void)
{
    const uint32_t sleep_tm_ms = m_ttlMs/2;

    while (true) {
        // Try to get lease
        int ret = 1;
        do {
            ret = m_masterLease->getLease(m_leaseKey, m_ttlMs);
            if (0 != ret) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        } while (0 != ret);

        // Switch to master mode
        m_currentRole.store(ProcessHARole::Master, std::memory_order_release);
        LOGI << "Promote to [MASTER] mode!"; 
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_tm_ms));

        // Try to renew lease periodically
        do {
            ret = m_masterLease->renewLease(m_leaseKey, m_ttlMs);
            if (0 == ret) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_tm_ms));
            } else {
                LOGE << "Downgrade to [SLAVE] mode!";
                m_currentRole.store(ProcessHARole::Slave, std::memory_order_release);
                if (nullptr != m_callback) {
                    m_callback();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                break;
            }
        } while (true);
    } // while (m_running)
}

bool MasterLeaseAgent::isMaster()
{
    return (ProcessHARole::Master == m_currentRole.load(std::memory_order_acquire));
}

}
