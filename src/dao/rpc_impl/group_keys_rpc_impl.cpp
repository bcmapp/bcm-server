#include "group_keys_rpc_impl.h"
#include "utils/log.h"

namespace bcm {
namespace dao {

std::atomic<uint64_t> GroupKeysRpcImpl::logId(0);

GroupKeysRpcImpl::GroupKeysRpcImpl(brpc::Channel* ch) : stub(ch) {}

ErrorCode GroupKeysRpcImpl::insert(const bcm::GroupKeys& groupKeys)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::SetGroupKeysReq request;
    bcm::dao::rpc::SetGroupKeysResp response;
    brpc::Controller cntl;

    bcm::GroupKeys* key = request.mutable_groupkeys();
    if (key == nullptr) {
        LOGE << "group key is null";
        return bcm::dao::ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
    *key = groupKeys;

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.setGroupKeys(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << " , req: " << request.Utf8DebugString();
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            CacheManager::getInstance()->set(groupKeys.gid(), groupKeys.version(), groupKeys);
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what() << " , req: " << request.Utf8DebugString();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupKeysRpcImpl::get(uint64_t gid, const std::set<int64_t>& versions, std::vector<bcm::GroupKeys>& groupKeys)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    std::set<int64_t> noneCachedVersions;
    int64_t max = 0;
    bool needCache = true;
    for (const auto& v : versions) {
        bcm::GroupKeys keys;
        if (!CacheManager::getInstance()->get(gid, v, keys)) {
            noneCachedVersions.emplace(v);
        } else {
            groupKeys.emplace_back(std::move(keys));
        }
        if (max < v) {
            max = v;
        }
    }

    if (noneCachedVersions.empty()) {
        return ErrorCode::ERRORCODE_SUCCESS;
    }

    if (noneCachedVersions.find(max) == noneCachedVersions.end()) {
        needCache = false;
    }

    bcm::dao::rpc::GetGroupKeysReq request;
    bcm::dao::rpc::GetGroupKeysResp response;
    brpc::Controller cntl;

    request.set_gid(gid);
    for (const auto& v : noneCachedVersions) {
        request.add_versions(v);
    }

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getGroupKeys(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << " , req: " << request.Utf8DebugString();
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            for (const auto& item : response.groupkeys()) {
                if (needCache && item.version() == max) {
                    CacheManager::getInstance()->set(item.gid(), item.version(), item);
                }
                groupKeys.emplace_back(std::move(item));
            }
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what() << " , req: " << request.Utf8DebugString();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupKeysRpcImpl::getLatestMode(uint64_t gid, bcm::GroupKeys::GroupKeysMode& mode)
{
    bcm::dao::rpc::LatestModeAndVersion mv;
    auto rc = getLatestModeAndVersion(gid, mv);
    if (rc == ErrorCode::ERRORCODE_SUCCESS) {
        mode = mv.mode();
    }
    return rc;
}

ErrorCode GroupKeysRpcImpl::getLatestModeBatch(const std::set<uint64_t>& gids, std::map<uint64_t /* gid */, bcm::GroupKeys::GroupKeysMode>& result)
{
    std::map<uint64_t, bcm::dao::rpc::LatestModeAndVersion> mvs;
    auto rc = getLatestModeAndVersionBatch(gids, mvs);
    if (rc == ErrorCode::ERRORCODE_SUCCESS) {
        for (const auto& item : mvs) {
            result.emplace(item.first, item.second.mode());
        }
    }
    return rc;
}

ErrorCode GroupKeysRpcImpl::clear(uint64_t gid)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::ClearGroupKeysReq request;
    bcm::dao::rpc::ClearGroupKeysResp response;
    brpc::Controller cntl;

    request.set_gid(gid);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.clear(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << " , req: " << request.Utf8DebugString();
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what() << " , req: " << request.Utf8DebugString();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupKeysRpcImpl::getLatestModeAndVersion(uint64_t gid, bcm::dao::rpc::LatestModeAndVersion& mv)
{
    std::map<uint64_t, bcm::dao::rpc::LatestModeAndVersion> result;
    std::set<uint64_t> gids = {gid};
    auto rc = getLatestModeAndVersionBatch(gids, result);
    if (rc == ErrorCode::ERRORCODE_SUCCESS) {
        auto it = result.find(gid);
        if (it == result.end()) {
            return ErrorCode::ERRORCODE_NO_SUCH_DATA;
        }
        mv = it->second;
    }
    return rc;
}

ErrorCode GroupKeysRpcImpl::getLatestGroupKeys(const std::set<uint64_t>& gids, std::vector<bcm::GroupKeys>& groupKeys)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetLatestGroupMessageKeysReq request;
    bcm::dao::rpc::GetLatestGroupMessageKeysResp response;
    brpc::Controller cntl;

    for (const auto& gid : gids) {
        request.add_gids(gid);
    }
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getLatestGroupKeys(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << " , req: " << request.Utf8DebugString();
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            groupKeys.assign(response.keys().begin(), response.keys().end());
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what() << " , req: " << request.Utf8DebugString();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupKeysRpcImpl::getLatestModeAndVersionBatch(const std::set<uint64_t>& gids, 
                                                         std::map<uint64_t, bcm::dao::rpc::LatestModeAndVersion>& result)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetLatestModeAndVersionBatchReq request;
    bcm::dao::rpc::GetLatestModeAndVersionBatchResp response;
    brpc::Controller cntl;

    for (const auto& gid : gids) {
        request.add_gids(gid);
    }
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getLatestModeAndVersionBatch(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << " , req: " << request.Utf8DebugString();
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            result.clear();
            for (const auto& mv : response.mvs()) {
                result[mv.gid()] = mv;
            }
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what() << " , req: " << request.Utf8DebugString();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

GroupKeysRpcImpl::CacheManager* GroupKeysRpcImpl::CacheManager::getInstance()
{
    static CacheManager instance;
    return &instance;
}

bool GroupKeysRpcImpl::CacheManager::get(uint64_t gid, int64_t version, bcm::GroupKeys& keys)
{
    if (m_bypass) {
        return false;
    }
    std::string k = cacheKey(gid, version);
    return m_caches.get(k, keys);
}

bool GroupKeysRpcImpl::CacheManager::set(uint64_t gid, int64_t version, const bcm::GroupKeys& keys)
{
    if (m_bypass) {
        return false;
    }
    std::string k = cacheKey(gid, version);
    return m_caches.set(k, keys);
}
    
std::string GroupKeysRpcImpl::CacheManager::cacheKey(uint64_t gid, int64_t version)
{
    std::ostringstream oss;
    oss << gid << "_" << version;
    return oss.str();
}

bool GroupKeysRpcImpl::CacheManager::getBypass() const
{
    return m_bypass;
}

void GroupKeysRpcImpl::CacheManager::setBypass(bool bypass)
{
    m_bypass = bypass;
}


}  // namespace dao
}  // namespace bcm
