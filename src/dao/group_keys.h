#pragma once

#include <string>
#include <vector>
#include <map>
#include "proto/dao/group_keys.pb.h"
#include "proto/brpc/rpc_group_keys.pb.h"
#include "proto/dao/error_code.pb.h"
#include <nlohmann/json.hpp>

namespace bcm {
namespace dao {

/*
 * Accounts dao virtual base class
 */
class GroupKeys {
public:
    /*
     * return ERRORCODE_SUCCESS if insert group keys success
     * corresponds to insertGroupKeys
     * insert include CAS process, only insert when (insert version > latest version)
     */
    virtual ErrorCode insert(const bcm::GroupKeys& groupKeys) = 0;

    virtual ErrorCode get(uint64_t gid, const std::set<int64_t>& versions, std::vector<bcm::GroupKeys>& groupKeys) = 0;

    virtual ErrorCode getLatestMode(uint64_t gid, bcm::GroupKeys::GroupKeysMode& mode) = 0;

    virtual ErrorCode getLatestModeBatch(const std::set<uint64_t>& gid, std::map<uint64_t /* gid */, bcm::GroupKeys::GroupKeysMode>& result) = 0;

    virtual ErrorCode getLatestModeAndVersion(uint64_t gid, bcm::dao::rpc::LatestModeAndVersion& mv) = 0;

    virtual ErrorCode getLatestGroupKeys(const std::set<uint64_t>& gids, std::vector<bcm::GroupKeys>& groupKeys) = 0;

    /*
     * return ERRORCODE_SUCCESS if del group keys success
     * corresponds to delGroup
     */
    virtual ErrorCode clear(uint64_t gid) = 0;


};

}  // namespace dao
}  // namespace bcm
