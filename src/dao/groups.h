#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "proto/dao/group.pb.h"
#include "proto/dao/error_code.pb.h"

namespace bcm {
namespace dao {

/*
 * Accounts dao virtual base class
 */
class Groups {
public:
    /*
     * return ERRORCODE_SUCCESS if create group success
     * corresponds to createNewGroup
     */
    virtual ErrorCode create(const bcm::Group& group, uint64_t& gid) = 0;

    /*
     * return ERRORCODE_SUCCESS if get group by id success
     * corresponds to queryGroupInfo
     */
    virtual ErrorCode get(uint64_t gid, bcm::Group& group) = 0;

    /*
     * return ERRORCODE_SUCCESS if update group success
     * corresponds to updateGroupTable
     */
    virtual ErrorCode update(uint64_t gid, const nlohmann::json& upData) = 0;

    /*
     * return ERRORCODE_SUCCESS if del group success
     * corresponds to delGroup
     */
    virtual ErrorCode del(uint64_t gid) = 0;

    virtual ErrorCode setGroupExtensionInfo(const uint64_t gid, const std::map<std::string, std::string>& info) = 0;

    virtual ErrorCode getGroupExtensionInfo(const uint64_t gid, const std::set<std::string>& extensionKeys,
                                            std::map<std::string, std::string>& info) = 0;
};

}  // namespace dao
}  // namespace bcm
