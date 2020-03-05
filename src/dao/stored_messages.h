#pragma once

#include <string>
#include "proto/dao/stored_message.pb.h"
#include "proto/dao/error_code.pb.h"

namespace bcm {
namespace dao {

const static uint32_t kMessageChunkSize = 100;

/*
 * StoredMessages dao virtual base class
 */
class StoredMessages {
public:

    /*
     * return ERRORCODE_SUCCESS if set message success
     */
    virtual ErrorCode set(const bcm::StoredMessage& msg , uint32_t& unreadMsgCount) = 0;

    /*
     * return ERRORCODE_SUCCESS if get msgs success
     * get at most maxCount msgs
     */
    virtual ErrorCode get(
            const std::string& destination
            , uint32_t destinationDeviceId
            , uint32_t maxCount
            , std::vector<bcm::StoredMessage>& msgs) = 0;

    /*
     * return ERRORCODE_SUCCESS if del message success
     */
    virtual ErrorCode del(
            const std::string& destination
            , const std::vector<uint64_t>& msgId) = 0;

    /*
     * return ERRORCODE_SUCCESS if clear msgs success
     */
    virtual ErrorCode clear(const std::string& destination) = 0;

    /*
     * return ERRORCODE_SUCCESS if clear msgs success
     */
    virtual ErrorCode clear(
            const std::string& destination
            , uint32_t destinationDeviceId) = 0;
};

}  // namespace dao
}  // namespace bcm
