#pragma once

#include <string>
#include <utils/jsonable.h>

namespace bcm {

namespace group {
    enum ErrorCode {
        ERRORCODE_SUCCESS = 0,
        ERRORCODE_PARAM_INCORRECT = 110001,
        ERRORCODE_INTERNAL_ERROR = 110002,
        ERRORCODE_NO_PERMISSION = 110003,
        ERRORCODE_NOT_FIND_USER = 110004,
        ERRORCODE_NO_SUCH_DATA = 11005,
        ERRORCODE_FORBIDDEN = 11006,
        ERRORCODE_UPGRADE_REQUIRED = 11007,
        ERRORCODE_NO_NEED = 110085
    };
    enum GroupVersion {
        GroupV0 = 0,
        GroupV3 = 3
    };
} // namespace group

struct GroupResponse {
    group::ErrorCode code{group::ERRORCODE_SUCCESS};
    std::string msg;
    nlohmann::json result{nlohmann::json::object()};

    GroupResponse()
        : code(group::ERRORCODE_SUCCESS)
        , msg("")
        , result(nlohmann::json::object())
    {}

    GroupResponse(const group::ErrorCode ecode, const std::string& strMsg)
        : code(ecode)
        , msg(strMsg)
        , result(nlohmann::json::object())
    {}
};

inline void to_json(nlohmann::json& j, const GroupResponse& response)
{
    j = nlohmann::json{
        {"error_code", static_cast<int32_t>(response.code)},
        {"error_msg", response.msg},
        {"result", response.result}
    };
}

inline void from_json(const nlohmann::json&, GroupResponse&)
{
}

}
