#pragma once

#include <utils/jsonable.h>
#include <list>
#include <vector>

namespace bcm {

enum {
    UNICAST = 1,
    BROADCAST
};

struct SysMsgContent {
    uint64_t id;
    uint64_t activityId;
    std::string type;
    std::string content;
};


inline void from_json(const nlohmann::json& j, SysMsgContent& msg)
{
    jsonable::toNumber(j, "id", msg.id);
    jsonable::toNumber(j, "activity_id", msg.activityId);
    jsonable::toString(j, "type", msg.type);
    jsonable::toString(j, "content", msg.content);
}

inline void to_json(nlohmann::json& j, const SysMsgContent& msg)
{
    j = nlohmann::json{
        {"id", msg.id},
        {"activity_id", msg.activityId},
        {"type", msg.type},
        {"content", msg.content}
    };
}

struct SysMsgRequest {
    int type;
    std::vector<std::string> destination_uids;
    SysMsgContent msg;
};

inline void to_json(nlohmann::json&, const SysMsgRequest&)
{
}

inline void from_json(const nlohmann::json& j, SysMsgRequest& request)
{
    jsonable::toNumber(j, "type", request.type);

    jsonable::toGeneric(j, "msg", request.msg);


    if (j.find("destination_uids") != j.end()) {
        jsonable::toGeneric(j, "destination_uids", request.destination_uids);
    } else {
        request.destination_uids = {};// set a default value
    }
}

struct SysMsgResponse {
    int32_t code = 0;
    std::string msg = "ok";
    std::vector<SysMsgContent> msgs;
};

inline void to_json(nlohmann::json& j, const SysMsgResponse& res)
{
    j = nlohmann::json{
        {"error_code", res.code},
        {"error_msg", res.msg},
        {"result", nlohmann::json{{"msgs", res.msgs}}}
    };
}

inline void from_json(const nlohmann::json&, SysMsgResponse&)
{

}

}
