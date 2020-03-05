#include <string>
#include "uid_checker.h"
#include "user_qps_limiter.h"
#include "limiter_manager.h"
#include "api_matcher.h"

namespace bcm {

UidChecker::UidChecker(std::shared_ptr<IApiMatcher>& apiMatcher) : m_apiMatcher(apiMatcher)
{
    auto ptr = LimiterManager::getInstance()->find(UserQpsLimiter::kIdentity);
    if (ptr == nullptr) {
        std::shared_ptr<UserQpsLimiter> limiter(new UserQpsLimiter());
        auto it = LimiterManager::getInstance()->emplace(limiter->identity(), limiter);
        LimiterConfigurationManager::getInstance()->registerObserver(limiter);
        ptr = it.first;
    }
    m_limiters.emplace_back(ptr);
}

CheckResult UidChecker::check(CheckArgs* args) 
{
    if (ignore(args)) {
        return CheckResult::PASSED;
    }

    preProcessing(args);

    if (args->uid.empty()) {
        return CheckResult::PASSED;
    }

    for (const auto& item : m_limiters) {
        if (item->acquireAccess(args->uid) != LimitLevel::GOOD) {
            return CheckResult::FAILED;
        }
    }
    
    return CheckResult::PASSED;
}

/**
 * all api with auth disabled:
 *  "/v1/accounts/challenge/:uid/get", //get from :uid
    "/v1/accounts/signup/put", //do have auth theader, but not validated
    "/v1/accounts/signin/put", //do have auth theader, but not validated
    "/v1/accounts/:uid/:signature/delete_", //get from :uid
    "/v1/accounts/sms/verification_code/:phonenumber/get",  // return with locked
    "/v1/accounts/bind_phonenumber/:phonenumber/:verification_code/get", // return precondition_failed
    "/v1/accounts/unbind_phonenumber/get", // return with ok
    "/v1/accounts/bind_phonenumber/put", // return with precondition_failed
    "/v1/attachments/upload/:attachmentId/put",
    "/v1/attachments/download/:attachmentId/get",
    "/v1/contacts/put", // return ok
    "/v1/contacts/token/:token/get", // return ok
    "/v1/contacts/tokens/put", // return ok
    "/v2/contacts/tokens/put", // return ok
    "/v2/contacts/tokens/users/get", // return ok
    "/v1/group/deliver/is_qr_code_valid/post",
    "/v1/group/deliver/query_uids/post", // return ok
    "/v1/keepalive/provisioning/get",  // return ok
    "/v1/profile/download/:avatarId/get",
    "/v1/system/push_system_message/post" // move to offline server
 */

bool UidChecker::ignore(const CheckArgs* args)
{
    /**
     * ignored api list
     *  "/v1/attachments/upload/:attachmentId/put",
        "/v1/attachments/download/:attachmentId/get",
        "/v1/group/deliver/is_qr_code_valid/post",
        "/v1/profile/download/:avatarId/get"
     */

    // static std::vector<Api> patterns = {
    //     Api(http::verb::put, "/v1/attachments/upload/:attachmentId"),
    //     Api(http::verb::get, "/v1/attachments/download/:attachmentId"),
    //     Api(http::verb::post, "/v1/group/deliver/is_qr_code_valid"), 
    //     Api(http::verb::get, "/v1/profile/download/:avatarId")
    // };

    // for (const auto& item : patterns) {
    //     if (m_apiMatcher->match(item, args->api)) {
    //         return true;
    //     }
    // }
    boost::ignore_unused(args);
    return false;
}

void UidChecker::preProcessing(CheckArgs* args)
{
    static std::vector<Api> patterns = {
        Api(http::verb::get, "/v1/accounts/challenge/:uid"),
        Api(http::verb::delete_, "/v1/accounts/:uid/:signature")
    };
    
    if (!args->uid.empty()) {
        return;
    }

    for (const auto& item : patterns) {
        if (m_apiMatcher->matchAndFetch(item, args->api, ":uid", args->uid)) {
            return;
        }
    }
    args->uid = "";
}

}