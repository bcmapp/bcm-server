
#include <map>

#include "utils/log.h"

#include "group_rpc_impl.h"
#include "../../proto/brpc/rpc_group.pb.h"


namespace bcm {
namespace dao {

std::atomic<uint64_t> GroupsRpcImp::logId(0);

GroupsRpcImp::GroupsRpcImp(brpc::Channel* ch) : stub(ch)
{

}

/*
 * return ERRORCODE_SUCCESS if create group success
 * corresponds to createNewGroup
 */
ErrorCode GroupsRpcImp::create(const bcm::Group& group, uint64_t& gid)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::CreateGroupReq request;
    bcm::dao::rpc::CreateGroupResp response;
    brpc::Controller cntl;

    bcm::Group* newGroup = request.mutable_group();
    *newGroup = group;
    gid = 0;

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.createGroup(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                << " , req: " << request.Utf8DebugString();
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            gid = response.groupid();
        }

        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what()
             << " , req: " << request.Utf8DebugString();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

/*
 * return ERRORCODE_SUCCESS if get group by id success
 * corresponds to queryGroupInfo
 */
ErrorCode GroupsRpcImp::get(uint64_t gid, bcm::Group& group)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetGroupByGroupIdReq request;
    bcm::dao::rpc::GetGroupByGroupIdResp response;
    brpc::Controller cntl;

    request.set_groupid(gid);

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getGroupInfoById(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << " , req: " << request.Utf8DebugString();
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            group = response.group();
        }

        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what()
             << " , req: " << request.Utf8DebugString();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

/*
 * return ERRORCODE_SUCCESS if update group success
 * corresponds to updateGroupTable
 */
ErrorCode GroupsRpcImp::update(uint64_t gid, const nlohmann::json& upData)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::UpdateGroupReq request;
    bcm::dao::rpc::UpdateGroupResp response;
    brpc::Controller cntl;

    std::string upStrData = upData.dump();

    request.set_groupid(gid);
    request.set_updatajson(upStrData);

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.updateGroup(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << " , req: " << request.Utf8DebugString();
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << ex.what()
             << " , req: " << request.Utf8DebugString();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

/*
 * return ERRORCODE_SUCCESS if del group success
 * corresponds to delGroup
 */
ErrorCode GroupsRpcImp::del(uint64_t gid)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::DeleteGroupByGroupIdReq request;
    bcm::dao::rpc::DeleteGroupByGroupIdResp response;
    brpc::Controller cntl;

    request.set_groupid(gid);

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.deleteGroup(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << " , req: " << request.Utf8DebugString();
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << ex.what()
             << " , req: " << request.Utf8DebugString();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupsRpcImp::setGroupExtensionInfo(const uint64_t gid, const std::map<std::string, std::string>& info)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::SetGroupExtensionReq  request;
    bcm::dao::rpc::SetGroupExtensionResp response;
    brpc::Controller cntl;

    request.set_groupid(gid);
    for (const auto& it : info) {
        bcm::dao::rpc::GroupExtensionPair* item = request.add_extensions();
        if (item == nullptr) {
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        item->set_key(it.first);
        item->set_value(it.second);
    }

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.setGroupExtensionInfo(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << " , req: " << request.Utf8DebugString();
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << ex.what()
             << " , req: " << request.Utf8DebugString();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode GroupsRpcImp::getGroupExtensionInfo(const uint64_t gid, const std::set<std::string>& extensionKeys,
                                              std::map<std::string, std::string>& info)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::GetGroupExtensionReq  request;
    bcm::dao::rpc::GetGroupExtensionResp response;
    brpc::Controller cntl;

    request.set_groupid(gid);
    for (const auto& it : extensionKeys) {
        std::string* item = request.add_extensionkeys();
        if (item == nullptr) {
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        *item = it;
    }

    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    try {
        stub.getGroupExtensionInfo(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode())
                 << " , req: " << request.Utf8DebugString();
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }

        ErrorCode ec = static_cast<ErrorCode>(response.rescode());
        if (ec == ErrorCode::ERRORCODE_SUCCESS) {
            for (const ::bcm::dao::rpc::GroupExtensionPair& ge : response.extensions()) {
                info.emplace(ge.key(), ge.value());
            }
        }

        return ec;
    } catch (const std::exception& ex) {
        LOGE << ex.what()
             << " , req: " << request.Utf8DebugString();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}


} // end namespace dao
} // end namespace bcm
