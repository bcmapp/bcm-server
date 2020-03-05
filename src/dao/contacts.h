#pragma once

#include <string>
#include "proto/dao/error_code.pb.h"

namespace bcm {
namespace dao {

enum class FriendEventType {
    FRIEND_REQUEST = 1,
    FRIEND_REPLY,
    DELETE_FRIEND
};

struct FriendEvent {
    int64_t id;
    std::string data;
};

/*
 * Contacts dao virtual base class
 */
class Contacts {
public:

    //------------------------------Deprecated Begin------------------------------------------
    /*
     * return ERRORCODE_SUCCESS if get contact success
     */
    virtual ErrorCode get(const std::string& uid, std::string& contacts) = 0;
    
    //------------------------------Deprecated End--------------------------------------------

    /*
     * return ERRORCODE_SUCCESS if get contact success
     */
    virtual ErrorCode getInParts(const std::string& uid, const std::vector<std::string>& parts, 
        std::map<std::string, std::string>& contacts) = 0;

    /*
     * return ERRORCODE_SUCCESS if set contact success
     */
    virtual ErrorCode setInParts(const std::string& uid, const std::map<std::string, std::string>& contactsInPart) = 0;


    virtual ErrorCode addFriendEvent(const std::string& uid, 
                                     FriendEventType eventType,
                                     const std::string& eventData, 
                                     int64_t& eventId) = 0;

    virtual ErrorCode getFriendEvents(const std::string& uid, 
                                      FriendEventType eventType,
                                      int count,
                                      std::vector<FriendEvent>& events) = 0;

    virtual ErrorCode delFriendEvents(const std::string& uid, 
                                      FriendEventType eventType,
                                      const std::vector<int64_t>& idList) = 0;
};

}  // namespace dao
}  // namespace bcm
