#pragma once

#include <brpc/channel.h>
#include <atomic>
#include "dao/utilities.h"
#include "proto/brpc/rpc_utilities.pb.h"

// UUID generator
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

namespace bcm {
namespace dao {

// Suppose there are two offline server processes P1 and P2, both of which 
// provide offline HA capabilities for the active/standby approach. 
// The lease call mode for both is (P1 and P2 share the same 'lease_key'):
/*
------------------------------------------------------------------------------------------------------------------
|                        P1                            |                          P2                             |
------------------------------------------------------------------------------------------------------------------
|                                                      |                                                         |
| // A global identity that represents the HA role     |   // A global identity that represents the HA role      |
| gHARole = kRoleUndefined;                            |   gHARole = kRoleUndefined;                             | 
|                                                      |                                                         |
| // A dedicated thread                                |   // A dedicated thread                                 |
| int ret = 1;                                         |   int ret = 1;                                          |
| do {                                                 |   do {                                                  |
|     ret = getLease(lease_key, ttl);                  |       ret = getLease(lease_key, ttl);                   |
|     if (0 != ret) {                                  |       if (0 != ret) {                                   |
|         sleep(n);                                    |           sleep(n);                                     |
|     }                                                |       }                                                 |
| } while(0 != ret);                                   |   } while(0 != ret);                                    |
|                                                      |                                                         |
| gHARole = kRoleActive;                               |   gHARole = kRoleActive;                                |
| sleep(n);                                            |   sleep(n);                                             |
|                                                      |                                                         |
| // periodically or on-demand renew lease             |   // periodically or on-demand renew lease              |
| do {                                                 |   do {                                                  |
|     ret = renewLease(lease_key, ttl);                |       ret = renewLease(lease_key, ttl);                 |
|     if (0 == ret) {                                  |       if (0 == ret) {                                   |
|         gHARole = kRoleActive;                       |           gHARole = kRoleActive;                        | 
|         sleep(n);                                    |           sleep(n);                                     |
|     } else {                                         |       } else {                                          |
|         gHARole = kRoleStandby;                      |           gHARole = kRoleStandby;                       |
|         // Stop some tasks                           |           // Stop some tasks                            |
|         // *** exit(0)                               |           // *** exit(0)                                |
|     }                                                |       }                                                 |
| } while(true);                                       |   } while(true);                                        |
------------------------------------------------------------------------------------------------------------------
*/
class MasterLeaseRpcImpl : public MasterLease  {
public:
    MasterLeaseRpcImpl(brpc::Channel* ch);

    virtual ErrorCode getLease(
        const std::string& key, 
        uint32_t ttlMs);

    virtual ErrorCode renewLease(
        const std::string& key,
        uint32_t ttlMs);           

    virtual ErrorCode releaseLease(
        const std::string& key);

private:
    static std::atomic<uint64_t> logId;
    bcm::dao::rpc::LeaseService_Stub stub;

    // UUID for each instance
    boost::uuids::uuid m_uuid;    
};

} // namespace dao
} // namespace bcm

