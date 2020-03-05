#pragma once

#include <string>
#include <vector>
#include "proto/dao/sys_msg.pb.h"
#include "proto/dao/error_code.pb.h"

namespace bcm {
namespace dao {

const uint32_t kMaxSysMsgSize = 16;
/*
 * StoredMessages dao virtual base class
 */
class SysMsgs {
public:

    /*
     * return ERRORCODE_SUCCESS if get msgs success
     * get at most kMaxSysMsgSize msgs order by msgId desc
     */
    virtual ErrorCode get(
            const std::string& destination
            , std::vector<bcm::SysMsg>& msgs
            , uint32_t maxMsgSize = kMaxSysMsgSize) = 0;

    /*
     * return ERRORCODE_SUCCESS if del message success
     * @Deprecated
     */
    virtual ErrorCode del(
            const std::string& destination
            , uint64_t msgId) = 0;

    /*
     * return ERRORCODE_SUCCESS if del message success
     */
    virtual ErrorCode delBatch(
            const std::string& destination
            , const std::vector<uint64_t>& msgIds) = 0;

    /*
     * return ERRORCODE_SUCCESS if clear msgs success
     * del all msgs for destination that is equal or less than maxMid
     */
    virtual ErrorCode delBatch(const std::string& destination, uint64_t maxMid) = 0;

    /*
     * return ERRORCODE_SUCCESS if insert message success
     */
    virtual ErrorCode insert(const bcm::SysMsg& msg) = 0;

    virtual ErrorCode insertBatch(const  std::vector<bcm::SysMsg>& msgs) = 0;

};

}  // namespace dao
}  // namespace bcm
