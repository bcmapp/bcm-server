#include "api_checker.h"
#include "limiter_manager.h"
#include "api_qps_limiter.h"
#include <string>
#include <boost/algorithm/string.hpp>

namespace bcm {

ApiChecker::ApiChecker(std::shared_ptr<IApiMatcher>& apiMatcher) 
    : m_apiMatcher(apiMatcher)
{
    auto ptr = LimiterManager::getInstance()->find(ApiQpsLimiter::kIdentity);
    if (ptr == nullptr) {
        std::shared_ptr<ApiQpsLimiter> limiter(new ApiQpsLimiter(m_apiMatcher));
        auto it = LimiterManager::getInstance()->emplace(limiter->identity(), limiter);
        LimiterConfigurationManager::getInstance()->registerObserver(limiter);
        ptr = it.first;
    }
    m_limiters.emplace_back(ptr);
}

CheckResult ApiChecker::check(CheckArgs* args) 
{
    std::string id("");
    getApiIdentity(args, id);
    if (id.empty()) {
        return CheckResult::PASSED;
    }

    for (const auto& item : m_limiters) {
        if (item->acquireAccess(id) != LimitLevel::GOOD) {
            return CheckResult::FAILED;
        }
    }

    return CheckResult::PASSED;
}

void ApiChecker::getApiIdentity(const CheckArgs* args, std::string& id)
{
    /**
     * need scpecial handling for the following api:
     * /v1/accounts/challenge/:uid
     * /v1/accounts/:uid/:signature
     * /v1/attachments/upload/:attachmentId
     * /v1/attachments/download/:attachmentId
     * /v1/attachments/:attachmentId
     * /v1/contacts/token/:token
     * /echo/:something
     * /v2/keys/:uid/:device_id
     * /v1/messages/:uid
     * /v1/profile/:uid
     * /v1/profile/namePlaintext/:name
     * /v1/profile/version/:version
     * /v1/profile/download/:avatarId
     * /v1/profile/nickname/:nickname
     * /v1/system/msgs/:mid
     */
    if (args->api.name.empty()) {
        id = "";
        return;
    }
    id = bcm::to_string(args->api);
}

}