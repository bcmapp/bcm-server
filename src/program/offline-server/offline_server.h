#pragma once

#include "offline_config.h"

#include <fiber/fiber_timer.h>

#include "store/accounts_manager.h"
#include "../../registers/offline_register.h"
#include "../../push/push_service.h"
#include "../../config/group_store_format.h"
#include "../../utils/lease_utils.h"

namespace bcm {

#define     OFFLINE_GROUP_MESSAGE_EXPIRE_TIME   30 * 60
#define     OFFLINE_GROUP_MESSAGE_DELAY_TIME    5
#define     OFFLINE_GROUP_MESSAGE_SCAN_SIZE     300
#define     OFFLINE_GROUP_USER_SCAN_SIZE        100
    
    
    struct GroupMessageIdInfo {
        uint64_t last_mid{0};
        uint32_t tm{0};
        int32_t  type{0};
        std::string  dbKey{""};
        int32_t  redisId{0};
        
        GroupMultibroadMessageInfo gmm;
    };
    
    struct GroupMessageInfoTask
    {
        uint64_t preRoundMid{0};     // Previous round group message id
        uint32_t preRoundMsgTs{0};   // Previous round group message timestamp
        int32_t  broadcastCount{0};
        int32_t  multicastCount{0};
        
        std::vector<GroupMessageIdInfo>     gms;
        std::set<std::string>  multicastMembers;
    };
    
class OfflineServiceImpl;

class OfflineService : public FiberTimer::Task {
public:
    OfflineService(OfflineConfig& config,
                   std::shared_ptr<AccountsManager> accountMgr,
                   std::shared_ptr<OfflineServiceRegister> offlineReg,
                   std::map<int32_t, RedisConfig>& redisDbHosts,
                   std::shared_ptr<push::Service> pushService);
    virtual ~OfflineService();

private:
    void run() override;
    void cancel() override;
    int64_t lastExecTimeInMilli() override;

    static void lostLease(void);
    
private:
    OfflineConfig& m_config;
    int64_t m_execTime;
    
    OfflineServiceImpl* m_pImpl;
    OfflineServiceImpl& m_impl;
    
    MasterLeaseAgent    m_masterLease;
};

}