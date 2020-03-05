#include "attachment_controller.h"
#include "attachment_entities.h"
#include "crypto/base64.h"
#include "crypto/hex_encoder.h"
#include "crypto/hmac.h"
#include "crypto/random.h"
#include "fiber/fiber_pool.h"
#include "utils/json_serializer.h"
#include "utils/log.h"
#include "utils/time.h"
#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <functional>
#include <iomanip>
#include <iostream>
#include <metrics_client.h>
#include <sstream>
#include "features/bcm_features.h"

namespace bcm {

using namespace metrics;

static constexpr char kMetricsAttachmentServiceName[] = "attachments";

AttachmentController::AttachmentController(
    const S3Config& s3Config,
    std::shared_ptr<AccountsManager> accountsManager)
    : m_s3Config(s3Config), m_accountsManager(accountsManager)
{
    // init mapping
    for (auto& mapping : m_s3Config.s3RegionMapping) {
        bool foundMapping = false;
        for (auto& bucket : m_s3Config.buckets) {
            if (mapping.s3Bucket == bucket.bucket) {
                std::vector<std::string> regions;
                boost::split(regions, mapping.lbsRegion, boost::is_any_of(","));
                for (auto& region : regions) {
                    boost::trim(region);
                    m_bucketMap.emplace(region, bucket);
                }
                foundMapping = true;
                break;
            }
        }
        if (!foundMapping) {
            LOGE << "S3 Configuration Fail, cannot found lbs region mapping bucket, lbs region is : " << mapping.lbsRegion;
            std::abort();
        }
    }

    // init default bucket
    bool found = false;
    for (auto& bucket : m_s3Config.buckets) {
        if (m_s3Config.defaultBucket == bucket.bucket) {
            m_defaultBucket = bucket;
            found = true;
            break;
        }
    }
    if (!found) {
        LOGE << "S3 Configuration Fail, cannot found default bucket";
        std::abort();
    }
}

void AttachmentController::addRoutes(HttpRouter& router)
{
    router.add(http::verb::post, "/v1/attachments/s3/upload_certification", Authenticator::AUTHTYPE_ALLOW_ALL,
               std::bind(&AttachmentController::signS3Upload, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<S3UploadCertificationRequest>, new JsonSerializerImp<S3UploadCertificationResponse>);
}

void AttachmentController::signS3Upload(HttpContext& context)
{
    int64_t dwStartTime = nowInMicro();
    auto* account = boost::any_cast<Account>(&context.authResult);
    auto& res = context.response;
    auto* uploadCertificationRequest = boost::any_cast<S3UploadCertificationRequest>(&context.requestEntity);

    // check type
    if (uploadCertificationRequest->type != "pmsg" &&
        uploadCertificationRequest->type != "gmsg" &&
        uploadCertificationRequest->type != "profile") {
        LOGW << "s3_upload_certification request type is wrong, param type: " << uploadCertificationRequest->type;
        res.result(http::status::bad_request);
        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAttachmentServiceName,
                                                             "s3_upload_certification", (nowInMicro() - dwStartTime), 1001);
        return;
    }

    // mapping bucket
    S3BucketInfo matchBucketInfo;
    auto regionSearch = m_bucketMap.find(uploadCertificationRequest->region);
    if (regionSearch == m_bucketMap.end()) {
        matchBucketInfo = m_defaultBucket;
        LOGI << account->uid() << " s3 upload_certification request region[" << uploadCertificationRequest->region << "] cannot map any bucket, use default instead";
    } else {
        matchBucketInfo = regionSearch->second;
    }

    // calculate expiration
    std::chrono::system_clock::time_point timePointNow = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point expirationTimePoint = timePointNow + std::chrono::minutes(m_s3Config.expirationInMinute);
    std::string expiration = getExpirationTime(expirationTimePoint);
    std::string day = getDay(timePointNow);
    std::string dayWithTime = getDayWithTime(timePointNow);

    // generate file name
    // file name is  {type)/{uuid}
    std::string filename = uploadCertificationRequest->type + "/" + generateUUID();

    auto& device = *(m_accountsManager->getAuthDevice(*account));
    auto& clientVer = device.clientversion();

    std::string feature_string = AccountsManager::getFeatures(*account, device.id());
    BcmFeatures features(HexEncoder::decode(feature_string));
    LOGT << "uid: " << account->uid() << ", feature: " << feature_string << ", os: " 
      << (clientVer.ostype() == ClientVersion::OSTYPE_IOS ? "ios" : "android") 
      << ", has feature: " << (features.hasFeature(bcm::Feature::FEATURE_ALIYUN_UPLOAD) ? "yes" : "no")
      << ", region: " << matchBucketInfo.region;

    // Workaround: fix for (IOS - aliyun OSS) bug, forward to aws s3
    if (clientVer.ostype() == ClientVersion::OSTYPE_IOS && !features.hasFeature(bcm::Feature::FEATURE_ALIYUN_UPLOAD)) {
        if ("cn" == matchBucketInfo.region) {
            matchBucketInfo = m_defaultBucket;
        }
    }

    if ("cn" == matchBucketInfo.region) { // aliyun oss
        // generate upload policy
        std::string uploadPolicy = getAliyunUploadPolicy(expiration, matchBucketInfo.bucket,
                                                         filename, std::to_string(m_s3Config.minFileSizeInBytes),
                                                         std::to_string(m_s3Config.maxFileSizeInBytes));

        // calculate sign url information
        std::string encodedPolicy = Base64::encode(uploadPolicy);
        std::string signature = Hmac::digest(Hmac::Algo::SHA1, m_s3Config.aliyunAccessSecurityKey, encodedPolicy);
        std::string encodedSignature = Base64::encode(signature);

        // response
        S3UploadCertificationResponse s3UploadCertificationResponse;

        s3UploadCertificationResponse.postUrl = matchBucketInfo.uploadUrl;
        std::string downloadUrl = matchBucketInfo.cdnUrl;
        if (downloadUrl.back() != '/') {
            downloadUrl = downloadUrl + "/";
        }
        downloadUrl += filename;
        s3UploadCertificationResponse.downloadUrl = downloadUrl;

        S3UploadCertificationResponseFields ossAccessKeyIdField{"OSSAccessKeyId", m_s3Config.aliyunAccessKey};
        S3UploadCertificationResponseFields policyField{"policy", encodedPolicy};
        S3UploadCertificationResponseFields signatureField{"Signature", encodedSignature};
        S3UploadCertificationResponseFields keyField{"key", filename};
        S3UploadCertificationResponseFields successActionStatusField{"success_action_status", "204"};

        s3UploadCertificationResponse.fields.emplace_back(ossAccessKeyIdField);
        s3UploadCertificationResponse.fields.emplace_back(policyField);
        s3UploadCertificationResponse.fields.emplace_back(signatureField);
        s3UploadCertificationResponse.fields.emplace_back(keyField);
        s3UploadCertificationResponse.fields.emplace_back(successActionStatusField);

        context.responseEntity = s3UploadCertificationResponse;
        res.result(http::status::ok);

        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAttachmentServiceName,
                                                             "s3_upload_certification", (nowInMicro() - dwStartTime), 0);

    } else { // aws s3
        // generate upload policy
        std::string uploadPolicy = getS3UploadPolicy(expiration, matchBucketInfo.bucket, m_s3Config.accessKey,
                                                     filename, std::to_string(m_s3Config.minFileSizeInBytes),
                                                     std::to_string(m_s3Config.maxFileSizeInBytes),
                                                     matchBucketInfo.region, day, dayWithTime);

        // calculate sign url information
        std::string encodedPolicy = Base64::encode(uploadPolicy);
        std::string dataKey = Hmac::digest(Hmac::Algo::SHA256, "AWS4" + m_s3Config.accessSecurityKey, day);
        std::string dateRegionKey = Hmac::digest(Hmac::Algo::SHA256, dataKey, matchBucketInfo.region);
        std::string dataRegionServiceKey = Hmac::digest(Hmac::Algo::SHA256, dateRegionKey, "s3");
        std::string signingKey = Hmac::digest(Hmac::Algo::SHA256, dataRegionServiceKey, "aws4_request");
        std::string signature = Hmac::digest(Hmac::Algo::SHA256, signingKey, encodedPolicy);
        std::string signatureHex = HexEncoder::encode(signature);

        // response
        S3UploadCertificationResponse s3UploadCertificationResponse;

        s3UploadCertificationResponse.postUrl = matchBucketInfo.uploadUrl;
        std::string downloadUrl = matchBucketInfo.cdnUrl;
        if (downloadUrl.back() != '/') {
            downloadUrl = downloadUrl + "/";
        }
        downloadUrl += filename;
        s3UploadCertificationResponse.downloadUrl = downloadUrl;
        S3UploadCertificationResponseFields keyField{"key", filename};
        S3UploadCertificationResponseFields credentialField{"X-Amz-Credential",
                                                            m_s3Config.accessKey + "/" + day + "/" + matchBucketInfo.region + "/s3/aws4_request"};
        S3UploadCertificationResponseFields dateField{"X-Amz-Date", dayWithTime};
        S3UploadCertificationResponseFields algorithmField{"X-Amz-Algorithm", "AWS4-HMAC-SHA256"};
        S3UploadCertificationResponseFields policyField{"Policy", encodedPolicy};
        S3UploadCertificationResponseFields signatureField{"X-Amz-Signature", signatureHex};

        s3UploadCertificationResponse.fields.emplace_back(keyField);
        s3UploadCertificationResponse.fields.emplace_back(credentialField);
        s3UploadCertificationResponse.fields.emplace_back(dateField);
        s3UploadCertificationResponse.fields.emplace_back(algorithmField);
        s3UploadCertificationResponse.fields.emplace_back(policyField);
        s3UploadCertificationResponse.fields.emplace_back(signatureField);

        context.responseEntity = s3UploadCertificationResponse;
        res.result(http::status::ok);

        MetricsClient::Instance()->markMicrosecondAndRetCode(kMetricsAttachmentServiceName,
                                                             "s3_upload_certification", (nowInMicro() - dwStartTime), 0);
    }
}

std::string AttachmentController::getS3UploadPolicy(const std::string& expiration,
                                                    const std::string& bucket,
                                                    const std::string& accessKey,
                                                    const std::string& fileName,
                                                    const std::string& minFileSize,
                                                    const std::string& maxFileSize,
                                                    const std::string& s3Region,
                                                    const std::string& day,
                                                    const std::string& dayWithTime)
{
    std::string policyStr = "{ \"expiration\": \"" + expiration + "\",\n"
                                                                  "  \"conditions\": [\n"
                                                                  "    {\"bucket\": \"" +
                            bucket + "\"},\n"
                                     "    [\"eq\", \"$key\", \"" +
                            fileName + "\"],\n"
                                       "    [\"content-length-range\", " +
                            minFileSize + ", " + maxFileSize + "],\n"
                                                               "    {\"x-amz-credential\": \"" +
                            accessKey + "/" + day + "/" + s3Region + "/s3/aws4_request\"},\n"
                                                                     "    {\"x-amz-algorithm\": \"AWS4-HMAC-SHA256\"},\n"
                                                                     "    {\"x-amz-date\": \"" +
                            dayWithTime + "\" }\n"
                                          "  ]\n"
                                          "}";
    return policyStr;
}

std::string AttachmentController::getAliyunUploadPolicy(const std::string& expiration,
                                                        const std::string& bucket,
                                                        const std::string& fileName,
                                                        const std::string& minFileSize,
                                                        const std::string& maxFileSize)
{
    std::string policyStr =
        "{ \"expiration\": \"" + expiration + "\",\n"
                                              "  \"conditions\": [\n"
                                              "    {\"bucket\": \"" +
        bucket + "\"},\n"
                 "    [\"eq\", \"$key\", \"" +
        fileName + "\"],\n"
                   "    [\"content-length-range\", " +
        minFileSize + ", " + maxFileSize + "]\n"
                                           "  ]\n"
                                           "}";
    return policyStr;
}

std::string AttachmentController::getExpirationTime(std::chrono::system_clock::time_point timePoint)
{
    auto itt = std::chrono::system_clock::to_time_t(timePoint);
    std::ostringstream ss;
    ss << std::put_time(gmtime(&itt), "%FT%T.000Z");
    return ss.str();
}

std::string AttachmentController::getDay(std::chrono::system_clock::time_point timePoint)
{
    auto itt = std::chrono::system_clock::to_time_t(timePoint);
    std::ostringstream ss;
    ss << std::put_time(gmtime(&itt), "%Y%m%d");
    return ss.str();
}

std::string AttachmentController::getDayWithTime(std::chrono::system_clock::time_point timePoint)
{
    auto itt = std::chrono::system_clock::to_time_t(timePoint);
    std::ostringstream ss;
    ss << std::put_time(gmtime(&itt), "%Y%m%dT%H%M%SZ");
    return ss.str();
}

std::string AttachmentController::generateUUID()
{
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    const std::string id = boost::uuids::to_string(uuid);
    const std::string realId = boost::algorithm::replace_all_copy(id, "-", "");
    return realId;
}

} // namespace bcm
