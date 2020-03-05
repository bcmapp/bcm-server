#pragma once

#include "utils/jsonable.h"
#include "proto/dao/account.pb.h"
#include <string>
#include <vector>

namespace bcm {

struct AttachmentDescripter {
    long id;
    std::string idString;
    std::string location;
};

inline void to_json(nlohmann::json& j, const AttachmentDescripter& attach)
{
    j = nlohmann::json{
        {"id", attach.id},
        {"idString", attach.idString},
        {"location", attach.location}
    };
}

inline void from_json(const nlohmann::json&, AttachmentDescripter&)
{
}

struct AttachmentUrlResult {
    std::string location;
};

inline void to_json(nlohmann::json& j, const AttachmentUrlResult& url)
{
    j = nlohmann::json{
        {"location", url.location}
    };
}

inline void from_json(const nlohmann::json&, AttachmentUrlResult&)
{
}

struct AttachmentUploadResult {
    uint32_t code{0};
    std::string msg;
    AttachmentUrlResult url;
};

inline void to_json(nlohmann::json& j, const AttachmentUploadResult& groupAvatar)
{
    j = nlohmann::json{
        {"error_code", groupAvatar.code},
        {"error_msg", groupAvatar.msg},
        {"result", groupAvatar.url}
    };
}

inline void from_json(const nlohmann::json&, AttachmentUploadResult&)
{
}


/**
 * s3
 */

struct S3UploadCertificationRequest {
    std::string type;
    std::string region;
};

inline void to_json(nlohmann::json&, const S3UploadCertificationRequest&)
{
}

inline void from_json(const nlohmann::json& j, S3UploadCertificationRequest& req)
{
    jsonable::toString(j, "type", req.type);
    jsonable::toString(j, "region", req.region);
}

struct S3UploadCertificationResponseFields {
    std::string key;
    std::string value;
};

inline void to_json(nlohmann::json& j, const S3UploadCertificationResponseFields& fields)
{
    j = nlohmann::json{
            {"key", fields.key},
            {"value", fields.value}
    };
}

inline void from_json(const nlohmann::json&, S3UploadCertificationResponseFields&)
{
}

struct S3UploadCertificationResponse {
    std::string postUrl;
    std::string downloadUrl;
    std::vector<S3UploadCertificationResponseFields> fields;
};

inline void to_json(nlohmann::json& j, const S3UploadCertificationResponse& req)
{
    j = nlohmann::json{
            {"postUrl", req.postUrl},
            {"downloadUrl", req.downloadUrl},
            {"fields", req.fields},
    };
}

inline void from_json(const nlohmann::json&, S3UploadCertificationResponse&)
{
}

} // namespace bcm
