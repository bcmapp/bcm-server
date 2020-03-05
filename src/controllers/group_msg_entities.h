#pragma once

#include <list>
#include <boost/core/ignore_unused.hpp>
#include "utils/jsonable.h"
#include "proto/dao/group_msg.pb.h"

namespace bcm {

// -----------------------------------------------------------------------------
// Section: GroupResponse
// -----------------------------------------------------------------------------
template <class Result>
struct GroupResponse {
    int errorCode{0};
    std::string errorMsg;
    Result result;
};

template <class Result>
inline void from_json(const nlohmann::json& j, GroupResponse<Result>& resp)
{
    boost::ignore_unused(j, resp);
}

template <class Result>
inline void to_json(nlohmann::json& j, const GroupResponse<Result>& resp)
{
    j["error_code"] = resp.errorCode;
    j["error_msg"] = resp.errorMsg;
    j["result"] = resp.result;
}

// -----------------------------------------------------------------------------
// Section: NullResult
// -----------------------------------------------------------------------------
struct GNullResult {};
inline void from_json(const nlohmann::json& j, GNullResult& res)
{
    boost::ignore_unused(j, res);
}

inline void to_json(nlohmann::json& j, const GNullResult& res)
{
    boost::ignore_unused(j, res);
}

// -----------------------------------------------------------------------------
// Section: GMsgRequest
// -----------------------------------------------------------------------------
struct GMsgRequest {
    uint64_t gid;
    std::string text;
    std::vector<std::string> atList;
    bool atAll;
    std::string pubKey;
    std::string sig;
};

inline void from_json(const nlohmann::json& j, GMsgRequest& req)
{
    jsonable::toNumber(j, "gid", req.gid);
    jsonable::toString(j, "text", req.text);
    jsonable::toGeneric(j, "at_list", req.atList, jsonable::OPTIONAL);
    jsonable::toString(j, "pub_key", req.pubKey, jsonable::OPTIONAL);
    jsonable::toString(j, "sig", req.sig, jsonable::OPTIONAL);

    int atAll = 0;
    jsonable::toNumber(j, "at_all", atAll, jsonable::OPTIONAL);
    req.atAll = (1 == atAll);
}

inline void to_json(nlohmann::json& j, const GMsgRequest& req)
{
    boost::ignore_unused(j, req);
}

// -----------------------------------------------------------------------------
// Section: GMsgResult
// -----------------------------------------------------------------------------
struct GMsgResult {
    uint64_t gid{0};
    uint64_t mid{0};
    uint64_t createTime{0};
};

inline void to_json(nlohmann::json& j, const GMsgResult& res)
{
    j["gid"] = res.gid;
    j["mid"] = res.mid;
    j["create_time"] = res.createTime;
}

// -----------------------------------------------------------------------------
// Section: GRecallRequest
// -----------------------------------------------------------------------------
struct GRecallRequest {
    uint64_t gid{0};
    uint64_t mid{0};
    std::string iv;
    std::string pubKey;
};

inline void from_json(const nlohmann::json& j, GRecallRequest& req)
{
    jsonable::toNumber(j, "gid", req.gid);
    jsonable::toNumber(j, "mid", req.mid);
    jsonable::toString(j, "iv", req.iv, jsonable::OPTIONAL);
    jsonable::toString(j, "pub_key", req.pubKey, jsonable::OPTIONAL);
}

inline void to_json(nlohmann::json& j, const GRecallRequest& req)
{
    boost::ignore_unused(j, req);
}

// -----------------------------------------------------------------------------
// Section: GGetMsgRequest
// -----------------------------------------------------------------------------
struct GGetMsgRequest {
    uint64_t gid;
    uint64_t from;
    uint64_t to;
};

inline void from_json(const nlohmann::json& j, GGetMsgRequest& req)
{
    jsonable::toNumber(j, "gid", req.gid);
    jsonable::toNumber(j, "from", req.from);
    jsonable::toNumber(j, "to", req.to);
}

inline void to_json(nlohmann::json& j, const GGetMsgRequest& req)
{
    boost::ignore_unused(j, req);
}

// -----------------------------------------------------------------------------
// Section: GMsgEntry
// -----------------------------------------------------------------------------
struct GMsgEntry {
    uint64_t gid;
    uint64_t mid;
    std::string fromUid;
    int type;
    std::string text;
    int status;
    std::string atList;
    uint64_t createTime;
    std::string sourceExtra;
};

inline void from_json(const nlohmann::json& j, GMsgEntry& msg)
{
    boost::ignore_unused(j, msg);
}

inline void to_json(nlohmann::json& j, const GMsgEntry& msg)
{
    j = nlohmann::json::object({
        {"gid", msg.gid},
        {"mid", msg.mid},
        {"from_uid", msg.fromUid},
        {"type", msg.type},
        {"text", msg.text},
        {"create_time", msg.createTime},
        {"source_extra", msg.sourceExtra}
    });
    if (GroupMsg::TYPE_CHAT == msg.type || GroupMsg::TYPE_CHANNEL == msg.type) {
        j["status"] = msg.status;
        j["at_list"] = nlohmann::json::parse(msg.atList);
    }
}

// -----------------------------------------------------------------------------
// Section: GGetMsgResult
// -----------------------------------------------------------------------------
struct GGetMsgResult {
    uint64_t gid{0};
    std::vector<GMsgEntry> messages;
};

inline void from_json(const nlohmann::json& j, GGetMsgResult& res)
{
    boost::ignore_unused(j, res);
}

inline void to_json(nlohmann::json& j, const GGetMsgResult& res)
{
    j["gid"] = res.gid;
    j["messages"] = res.messages;
}

// -----------------------------------------------------------------------------
// Section: GAckMsgRequest
// -----------------------------------------------------------------------------
struct GAckMsgRequest {
    uint64_t gid;
    uint64_t lastMid;
};

inline void from_json(const nlohmann::json& j, GAckMsgRequest& req)
{
    jsonable::toNumber(j, "gid", req.gid);
    jsonable::toNumber(j, "last_mid", req.lastMid);
}

inline void to_json(nlohmann::json& j, const GAckMsgRequest& req)
{
    boost::ignore_unused(j, req);
}

// -----------------------------------------------------------------------------
// Section: GLastMidEntry
// -----------------------------------------------------------------------------
struct GLastMidEntry {
    uint64_t gid;
    uint64_t lastMid;
    uint64_t lastAckMid;
};

inline void from_json(const nlohmann::json& j, GLastMidEntry& ent)
{
    boost::ignore_unused(j, ent);
}

inline void to_json(nlohmann::json& j, const GLastMidEntry& ent)
{
    j = {
        {"gid", ent.gid},
        {"last_mid", ent.lastMid},
        {"last_ack_mid", ent.lastAckMid}
    };
}

// -----------------------------------------------------------------------------
// Section: GQueryLastMidResult
// -----------------------------------------------------------------------------
struct GQueryLastMidResult {
    std::vector<GLastMidEntry> groups;
};

inline void from_json(const nlohmann::json& j, GQueryLastMidResult& res)
{
    boost::ignore_unused(j, res);
}

inline void to_json(nlohmann::json& j, const GQueryLastMidResult& res)
{
    j["groups"] = res.groups;
}

// -----------------------------------------------------------------------------
// Section: GQueryUidsRequest
// -----------------------------------------------------------------------------
struct GQueryUidsRequest {
    uint64_t gid;
    std::string type;
};

inline void from_json(const nlohmann::json& j, GQueryUidsRequest& msg)
{
    jsonable::toNumber(j, "gid", msg.gid);
    jsonable::toString(j, "type", msg.type);
}

inline void to_json(nlohmann::json& j, const GQueryUidsRequest& msg)
{
    boost::ignore_unused(j, msg);
}

// -----------------------------------------------------------------------------
// Section: GQueryUidsResult
// -----------------------------------------------------------------------------
struct GQueryUidsResult {
    int total;
    int totalOnline;
    std::vector<std::string> uids;  // all member uids
};

inline void from_json(const nlohmann::json& j, GQueryUidsResult& res)
{
    boost::ignore_unused(j, res);
}

inline void to_json(nlohmann::json& j, const GQueryUidsResult& res)
{
    j["total"] = res.total;
    j["online_total"] = res.totalOnline;
    j["uids"] = res.uids;
}

}