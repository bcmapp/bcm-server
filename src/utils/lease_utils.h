#pragma once

#include "dao/utilities.h"
#include <functional>
#include <string>
#include <pthread.h>
#include <atomic>
#include <future>

namespace bcm {

// Depending on the availability of lease, the different role 
// definitions of the current process in HA mode
enum class ProcessHARole {
    Undefined, 
    Master, 
    Slave
};

// The caller should define its own callback
typedef std::function<void(void)> leaseLostCallback_t;

class MasterLeaseAgent {
public:
    MasterLeaseAgent(
        const std::string& key, 
        uint32_t ttlMs, 
        leaseLostCallback_t cb);
    
    void start(void);
    void stop(void);
    bool isMaster(void);

private:
    // Internal thread loop
    void loop(void);

private:
    std::string m_leaseKey;
    uint32_t m_ttlMs;
    std::shared_ptr<dao::MasterLease> m_masterLease;
    std::atomic<ProcessHARole> m_currentRole;
    leaseLostCallback_t m_callback;
    pthread_t m_tid;
};

} // namespace
