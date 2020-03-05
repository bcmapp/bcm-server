#pragma once

#include <string>
#include <vector>
#include <utils/jsonable.h>

namespace bcm {

// -----------------------------------------------------------------------------
// Section: S3BucketInfo
// -----------------------------------------------------------------------------
struct S3BucketInfo {
    std::string region;
    std::string bucket;
    std::string cdnUrl;
    std::string uploadUrl;
};

inline void to_json(nlohmann::json& j, const S3BucketInfo& e)
{
    j = nlohmann::json{{"region", e.region},
                       {"bucket", e.bucket},
                       {"cdnUrl", e.cdnUrl},
                       {"uploadUrl", e.uploadUrl}};
}

inline void from_json(const nlohmann::json& j, S3BucketInfo& e)
{
    jsonable::toString(j, "region", e.region);
    jsonable::toString(j, "bucket", e.bucket);
    jsonable::toString(j, "cdnUrl", e.cdnUrl);
    jsonable::toString(j, "uploadUrl", e.uploadUrl);
}

// -----------------------------------------------------------------------------
// Section: S3BucketInfo
// -----------------------------------------------------------------------------
struct S3RegionMapping {
    std::string lbsRegion;
    std::string s3Bucket;
};

inline void to_json(nlohmann::json& j, const S3RegionMapping& e)
{
    j = nlohmann::json{{"lbsRegion", e.lbsRegion},
                       {"s3Bucket", e.s3Bucket}};
}

inline void from_json(const nlohmann::json& j, S3RegionMapping& e)
{
    jsonable::toString(j, "lbsRegion", e.lbsRegion);
    jsonable::toString(j, "s3Bucket", e.s3Bucket);
}

// -----------------------------------------------------------------------------
// Section: S3Config
// -----------------------------------------------------------------------------
struct S3Config {
    std::string accessKey;
    std::string accessSecurityKey;
    std::string aliyunAccessKey;
    std::string aliyunAccessSecurityKey;
    uint64_t iosRedirectSupportVersion; 
    uint64_t minFileSizeInBytes;
    uint64_t maxFileSizeInBytes;
    uint64_t expirationInMinute;
    std::string defaultBucket;
    std::vector<S3BucketInfo> buckets;
    std::vector<S3RegionMapping> s3RegionMapping;
};

inline void to_json(nlohmann::json& j, const S3Config& config)
{
    j = nlohmann::json{{"accessKey", config.accessKey},
                       {"accessSecurityKey", config.accessSecurityKey},
                       {"aliyunAccessKey", config.aliyunAccessKey},
                       {"aliyunAccessSecurityKey", config.aliyunAccessSecurityKey},
                       {"iosRedirectSupportVersion", config.iosRedirectSupportVersion},
                       {"minFileSizeInBytes", config.minFileSizeInBytes},
                       {"maxFileSizeInBytes", config.maxFileSizeInBytes},
                       {"expirationInMinute", config.expirationInMinute},
                       {"defaultBucket", config.defaultBucket},
                       {"buckets", config.buckets},
                       {"s3RegionMapping", config.s3RegionMapping}};
}

inline void from_json(const nlohmann::json& j, S3Config& config)
{
    jsonable::toString(j, "accessKey", config.accessKey);
    jsonable::toString(j, "accessSecurityKey", config.accessSecurityKey);
    jsonable::toString(j, "aliyunAccessKey", config.aliyunAccessKey);
    jsonable::toString(j, "aliyunAccessSecurityKey", config.aliyunAccessSecurityKey);
    jsonable::toNumber(j, "iosRedirectSupportVersion", config.iosRedirectSupportVersion); 
    jsonable::toNumber(j, "minFileSizeInBytes", config.minFileSizeInBytes);
    jsonable::toNumber(j, "maxFileSizeInBytes", config.maxFileSizeInBytes);
    jsonable::toNumber(j, "expirationInMinute", config.expirationInMinute);
    jsonable::toString(j, "defaultBucket", config.defaultBucket);
    jsonable::toGeneric(j, "buckets", config.buckets);
    jsonable::toGeneric(j, "s3RegionMapping", config.s3RegionMapping);
}


}