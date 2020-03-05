#pragma once

#include <brpc/channel.h>
#include <atomic>
#include "../sys_msgs.h"
#include "../../proto/dao/sys_msg.pb.h"
#include "../../proto/brpc/rpc_sys_messages.pb.h"

namespace bcm {
namespace dao {

class SysMsgsRpcImpl : public SysMsgs {
public:
    SysMsgsRpcImpl(brpc::Channel* ch);

    /*
     * return ERRORCODE_SUCCESS if get msgs success
     * get at most kMaxSysMsgSize msgs order by msgId desc
     */
    virtual ErrorCode get(
        const std::string& destination
        , std::vector<bcm::SysMsg>& msgs
        , uint32_t maxMsgSize = kMaxSysMsgSize);

    /*
     * return ERRORCODE_SUCCESS if del message success
     * @Deprecated
     */
    virtual ErrorCode del(
        const std::string& destination
        , uint64_t msgId);

    /*
     * return ERRORCODE_SUCCESS if del message success
     */
    virtual ErrorCode delBatch(
        const std::string& destination
        , const std::vector<uint64_t>& msgIds);

    /*
     * return ERRORCODE_SUCCESS if clear msgs success
     * del all msgs for destination that is equal or less than maxMid
     */
    virtual ErrorCode delBatch(const std::string& destination, uint64_t maxMid);

    /*
     * return ERRORCODE_SUCCESS if insert message success
     */
    virtual ErrorCode insert(const bcm::SysMsg& msg);

    virtual ErrorCode insertBatch(const  std::vector<bcm::SysMsg>& msgs);
private:
    static std::atomic<uint64_t> logId;
    bcm::dao::rpc::SysMessagesService_Stub stub;

};

} // namespace dao
}  // namespace bcm

