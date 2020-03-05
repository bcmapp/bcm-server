#pragma once

#include "http/http_router.h"
#include "config/s3_config.h"
#include <store/accounts_manager.h>
#include <chrono>

namespace bcm {

class AttachmentController : public std::enable_shared_from_this<AttachmentController>
                             , public HttpRouter::Controller {
public:

    AttachmentController(
        const S3Config& s3Config, 
        std::shared_ptr<AccountsManager> accountsManager);

    ~AttachmentController() = default;

    void addRoutes(HttpRouter& router) override;

public:
    void signS3Upload(HttpContext& context);

private:

    std::string generateUUID();
    std::string getS3UploadPolicy(const std::string& expiration,
                                  const std::string& bucket,
                                  const std::string& accessKey,
                                  const std::string& fileName,
                                  const std::string& minFileSize,
                                  const std::string& maxFileSize,
                                  const std::string& s3Region, const std::string& day,
                                  const std::string& dayWithTime);

    std::string getAliyunUploadPolicy(const std::string& expiration,
                                  const std::string& bucket,
                                  const std::string& fileName,
                                  const std::string& minFileSize,
                                  const std::string& maxFileSize);

    std::string getExpirationTime(std::chrono::system_clock::time_point timePoint);
    std::string getDay(std::chrono::system_clock::time_point timePoint);
    std::string getDayWithTime(std::chrono::system_clock::time_point timePoint);

private:

    std::map<std::string /* lbsRegion */, S3BucketInfo> m_bucketMap;
    S3Config m_s3Config;
    S3BucketInfo m_defaultBucket;
    std::shared_ptr<AccountsManager> m_accountsManager;

private:

    static const std::string kAttachmentPrefix;

};

} // namespace bcm
