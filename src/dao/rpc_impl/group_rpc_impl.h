#pragma once

#include <brpc/channel.h>
#include "dao/groups.h"
#include "proto/brpc/rpc_group.pb.h"
#include <memory>
#include <atomic>

namespace bcm {
namespace dao {


class GroupsRpcImp : public Groups {
public:
    GroupsRpcImp(brpc::Channel* ch);

    /*
     * return ERRORCODE_SUCCESS if create group success
     * corresponds to createNewGroup
     */
    virtual ErrorCode create(const bcm::Group& group, uint64_t& gid);

    /*
     * return ERRORCODE_SUCCESS if get group by id success
     * corresponds to queryGroupInfo
     */
    virtual ErrorCode get(uint64_t gid, bcm::Group& group);

    /*
     * return ERRORCODE_SUCCESS if update group success
     * corresponds to updateGroupTable
     */
    virtual ErrorCode update(uint64_t gid, const nlohmann::json& upData);

    /*
     * return ERRORCODE_SUCCESS if del group success
     * corresponds to delGroup
     */
    virtual ErrorCode del(uint64_t gid);

    virtual ErrorCode setGroupExtensionInfo(const uint64_t gid, const std::map<std::string, std::string>& info);

    virtual ErrorCode getGroupExtensionInfo(const uint64_t gid, const std::set<std::string>& extensionKeys,
                                            std::map<std::string, std::string>& info);

private:
    static std::atomic<uint64_t> logId;
    bcm::dao::rpc::GroupsService_Stub stub;
};

}  // namespace dao
}  // namespace bcm

