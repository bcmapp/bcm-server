#pragma once

#include <proto/dao/pending_group_user.pb.h>
#include "proto/dao/error_code.pb.h"

namespace bcm {
namespace dao {

class PendingGroupUsers {
public:
    /**
     * @brief
     * @param user
     * @return - ERRORCODE_SUCCESS if sucess, otherwise fail
     */
    virtual ErrorCode set(const PendingGroupUser& user) = 0;

    /**
     * @brief load maxmium @count pending group users that start from @startUid
     * @param gid
     * @param startUid
     * @param count
     * @param result
     * @return - ERRORCODE_SUCCESS if sucess, otherwise fail
     */
    virtual ErrorCode query(uint64_t gid,
                            const std::string& startUid,
                            int count,
                            std::vector<PendingGroupUser>& result) = 0;

    /**
     * @brief
     * @param gid
     * @param uids
     * @return - ERRORCODE_SUCCESS if sucess, otherwise fail
     */
    virtual ErrorCode del(uint64_t gid, std::set<std::string> uids) = 0;

    /**
     * @brief
     * @param gid
     * @return
     */
    virtual ErrorCode clear(uint64_t gid) = 0;
};

}
}
