#pragma once

#include <brpc/channel.h>
#include "dao/contacts.h"
#include "../../proto/brpc/rpc_contact.pb.h"
#include <memory>
#include <atomic>

namespace bcm {
namespace dao {

class ContactsRpcImp : public Contacts {
public:
    ContactsRpcImp(brpc::Channel* ch);

    //------------------------------Deprecated Begin------------------------------------------
    virtual ErrorCode get(const std::string& uid, std::string& contacts);
    
    //------------------------------Deprecated End--------------------------------------------

    virtual ErrorCode getInParts(const std::string& uid, const std::vector<std::string>& parts,
                                 std::map<std::string, std::string>& contacts);
    virtual ErrorCode setInParts(const std::string& uid, const std::map<std::string, std::string>& contactsInPart);

    virtual ErrorCode addFriendEvent(const std::string& uid, 
                                     FriendEventType eventType,
                                     const std::string& eventData, 
                                     int64_t& eventId);

    virtual ErrorCode getFriendEvents(const std::string& uid, 
                                      FriendEventType eventType, int count, 
                                      std::vector<FriendEvent>& events);

    virtual ErrorCode delFriendEvents(const std::string& uid, 
                                      FriendEventType eventType,
                                      const std::vector<int64_t>& idList);

private:
    static std::atomic<uint64_t> logId;
    bcm::dao::rpc::ContactService_Stub stub;

    std::string m_NullToken;
};

}  // namespace dao
}  // namespace bcm

