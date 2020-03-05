#include "contacts_rpc_impl.h"
#include "utils/log.h"

#include <map>


namespace bcm {
namespace dao {

ContactsRpcImp::ContactsRpcImp(brpc::Channel* ch) : stub(ch), m_NullToken("[]")
{

}

ErrorCode ContactsRpcImp::get(const std::string& uid, std::string& contacts)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetContactReq request;
    bcm::dao::rpc::GetContactResp response;
    brpc::Controller cntl;

    request.set_uid(uid);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getContact(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            contacts = response.contact();
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode ContactsRpcImp::getInParts(const std::string& uid, const std::vector<std::string>& parts,
                             std::map<std::string, std::string>& contacts)
{
    if (stub.channel() == nullptr) {
        LOGE << " uid: " << uid << ", stub.channel() == nullptr " ;
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetContactInPartsReq request;
    bcm::dao::rpc::GetContactInPartsResp response;
    brpc::Controller cntl;

    request.set_uid(uid);

    if (parts.size() == 0) {
        LOGE << " uid: " << uid << ", parts == nullptr " ;
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    for (const auto& part : parts) {
        request.add_parts(part);
    }

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getInParts(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << " uid: " << uid << ", ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            for (const ::bcm::dao::rpc::ContactPartInfo& it : response.contacts()) {
                contacts[it.partkey()] = it.partvalue();
            }
        }
        return ec;
    } catch (const std::exception& ex) {
        LOGE << "uid: " << uid << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode ContactsRpcImp::setInParts(const std::string& uid, const std::map<std::string, std::string>& contactsInPart)
{
    if (stub.channel() == nullptr) {
        LOGE << " uid: " << uid << ", stub.channel() == nullptr " ;
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::SetContactInPartsReq request;
    bcm::dao::rpc::SetContactInPartsResp response;
    brpc::Controller cntl;

    request.set_uid(uid);

    for (const auto& itCont : contactsInPart) {
        ::bcm::dao::rpc::ContactPartInfo* it = request.add_contacts();
        if (it) {
            it->set_partkey(itCont.first);
            it->set_partvalue(itCont.second);
        } else {
            LOGE << " uid: " << uid << ", add_contacts == nullptr " ;
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
    }

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.setInParts(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << " uid: " << uid << "ErrorCode: " << cntl.ErrorCode()
                 << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode ContactsRpcImp::addFriendEvent(const std::string& uid,
                                         FriendEventType eventType,
                                         const std::string& eventData,
                                         int64_t& eventId)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::SetFriendshipReq request;
    bcm::dao::rpc::SetFriendshipResp response;
    brpc::Controller cntl;

    request.set_uid(uid);
    request.set_type(static_cast<int>(eventType));
    request.set_data(eventData);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.setFriendship(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << " uid: " << uid << ", ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        eventId = response.id();
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
    return static_cast<ErrorCode>(response.rescode());
}

ErrorCode ContactsRpcImp::getFriendEvents(const std::string& uid,
                                          FriendEventType eventType,
                                          int count,
                                          std::vector<FriendEvent>& events)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::QueryFriendshipReq request;
    bcm::dao::rpc::QueryFriendshipResp response;
    brpc::Controller cntl;

    request.set_uid(uid);
    request.set_type(static_cast<int>(eventType));
    request.set_count(count);
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.queryFriendship(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << " uid: " << uid << ", ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        for (const auto& item : response.data()) {
            FriendEvent fe;
            fe.data = item.data();
            fe.id = item.id();
            events.emplace_back(fe);
        }
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
    return static_cast<ErrorCode>(response.rescode());
}

ErrorCode ContactsRpcImp::delFriendEvents(const std::string& uid,
                                          FriendEventType eventType,
                                          const std::vector<int64_t>& idList)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::DelFriendshipReq request;
    bcm::dao::rpc::DelFriendshipResp response;
    brpc::Controller cntl;

    request.set_uid(uid);
    request.set_type(static_cast<int>(eventType));
    for (const auto& id : idList) {
        request.add_ids(id);
    }
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.delFriendship(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << " uid: " << uid << ", ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
    return static_cast<ErrorCode>(response.rescode());
}


std::atomic<uint64_t> ContactsRpcImp::logId(0);

} // end namespace dao
} // end namespace bcm

