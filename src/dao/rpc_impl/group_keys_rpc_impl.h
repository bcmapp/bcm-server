#include <brpc/channel.h>
#include <memory>
#include <atomic>
#include "dao/group_keys.h"
#include "proto/brpc/rpc_group_keys.pb.h"
#include "dao/dao_cache/fifo_cache.h"
#include "config/bcm_options.h"

#ifdef UNIT_TEST
#define private public
#define protected public
#endif
namespace bcm {
namespace dao {


class GroupKeysRpcImpl : public GroupKeys {
public:
    GroupKeysRpcImpl(brpc::Channel* ch);

    virtual ErrorCode insert(const bcm::GroupKeys& groupKeys) override;

    virtual ErrorCode get(uint64_t gid, 
                          const std::set<int64_t>& versions, 
                          std::vector<bcm::GroupKeys>& groupKeys) override;

    virtual ErrorCode getLatestMode(uint64_t gid, bcm::GroupKeys::GroupKeysMode& mode) override;

    virtual ErrorCode getLatestModeBatch(const std::set<uint64_t>& gid, 
                                         std::map<uint64_t, bcm::GroupKeys::GroupKeysMode>& result) override;

    virtual ErrorCode clear(uint64_t gid) override;

    virtual ErrorCode getLatestModeAndVersion(uint64_t gid, bcm::dao::rpc::LatestModeAndVersion& mv) override;

    virtual ErrorCode getLatestGroupKeys(const std::set<uint64_t>& gids, std::vector<bcm::GroupKeys>& groupKeys) override;

protected:
    class CacheManager {
    public:
        static CacheManager* getInstance();
        bool get(uint64_t gid, int64_t version, bcm::GroupKeys& keys);
        bool set(uint64_t gid, int64_t version, const bcm::GroupKeys& keys);

        bool getBypass() const;
        void setBypass(bool bypass);
    
    private:
        CacheManager() : m_caches(bcm::BcmOptions::getInstance()->getConfig().cacheConfig.groupKeysLimit),
                         m_bypass(false) {}
        std::string cacheKey(uint64_t gid, int64_t version);
    private:
        FIFOCache<std::string, bcm::GroupKeys> m_caches;
        bool m_bypass;
    };

private:
    ErrorCode getLatestModeAndVersionBatch(const std::set<uint64_t>& gids, 
                                           std::map<uint64_t, bcm::dao::rpc::LatestModeAndVersion>& result);
    static std::atomic<uint64_t> logId;
    bcm::dao::rpc::GroupKeyService_Stub stub;
};

}  // namespace dao
}  // namespace bcm

#ifdef UNIT_TEST
#undef private
#undef protected
#endif
