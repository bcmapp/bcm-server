#include "limiter_executor.h"
#include "api_checker.h"
#include "uid_checker.h"
#include "api_matcher.h"
#include "http/custom_http_status.h"
#include "auth/authorization_header.h"
#include "limiter_globals.h"

namespace bcm{

LimiterExecutor::LimiterExecutor() : m_apiMatcher(new ApiMatcher())
{
    m_checkers.emplace_back(std::make_shared<UidChecker>(m_apiMatcher));
    m_checkers.emplace_back(std::make_shared<ApiChecker>(m_apiMatcher));
}

LimiterExecutor::~LimiterExecutor() {}

bool LimiterExecutor::validate(const ValidateInfo& info, uint32_t& status)
{
    if (info.request == nullptr) {
        return true;
    }
    auto& request = *info.request;
    CheckArgs args;
    std::string url = request.target().to_string();
    auto queryStartPos = url.find('?');
    if (queryStartPos != std::string::npos) {
        args.api.name.assign(url.c_str(), 0, queryStartPos);
    } else {
        args.api.name = url;
    }
    args.api.method = request.method();
    auto authHeader = AuthorizationHeader::parse(request[http::field::authorization].to_string());
    if (authHeader) {
        args.uid = authHeader->uid();
    }
    args.origin = info.origin;

    if (ignore(args.api)) {
        return true;
    }

    for (auto& checker : m_checkers) {
        auto result = checker->check(&args);
        if (result == CheckResult::ABORT) {
            return true;
        }
        if (result == CheckResult::FAILED) {
            status = static_cast<unsigned>(bcm::custom_http_status::limiter_rejected);
            return false;
        }
    }
    return true;
}

bool LimiterExecutor::ignore(const Api& api)
{
    /**
     * list of api to ignore:
        "/v1/accounts/sms/verification_code/:phonenumber/get",  // return with locked
        "/v1/accounts/bind_phonenumber/:phonenumber/:verification_code/get", // return precondition_failed
        "/v1/accounts/unbind_phonenumber/get", // return with ok
        "/v1/accounts/bind_phonenumber/put", // return with precondition_failed
        "/v1/contacts/put", // return ok
        "/v1/contacts/token/:token/get", // return ok
        "/v1/contacts/tokens/put", // return ok
        "/v2/contacts/tokens/put", // return ok
        "/v2/contacts/tokens/users/get", // return ok
        "/v1/group/deliver/query_uids/post", // return ok
        "/v1/keepalive/provisioning/get",  // return ok
        "/v1/system/push_system_message/post" // move to offline server
    */

    for (const auto& item : LimiterGlobals::getInstance()->getIgnoredApis()) {
        if (m_apiMatcher->match(item, api)) {
            return true;
        }
    }
    return false;
}

}
