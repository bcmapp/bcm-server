#pragma once

#include <list>
#include <boost/core/ignore_unused.hpp>
#include <utils/jsonable.h>

namespace bcm {

const std::string kOfflinePushMessageUrl = "/v1/offline/pushmsg";

struct GroupOfflinePushMessage {
    std::string groupId;
    std::string messageId;
    std::map<std::string /* uid */, std::string /* json::GroupUserMessageIdInfo */ > destinations;
};

inline void to_json(nlohmann::json& j, const GroupOfflinePushMessage& msg)
{
    j = nlohmann::json{{"gid", msg.groupId},
                       {"mid", msg.messageId},
                       {"destinations", msg.destinations}};
}

inline void from_json(const nlohmann::json& j, GroupOfflinePushMessage& msg)
{
    jsonable::toString(j, "gid", msg.groupId);
    jsonable::toString(j, "mid", msg.messageId);
    jsonable::toGeneric(j, "destinations", msg.destinations);
}

}

