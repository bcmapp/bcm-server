#include "limiter_globals.h"
#include "utils/log.h"

namespace bcm {

std::shared_ptr<LimiterGlobals> LimiterGlobals::getInstance() 
{
    static std::shared_ptr<LimiterGlobals> instance(new LimiterGlobals);
    return instance;
}

void LimiterGlobals::update(const Api& api)
{
    if (kActiveApis.find(api) == kActiveApis.end()) {
        m_ignoredApis.emplace(api);
    }
}

#if 0
void LimiterGlobals::dump()
{
    auto out = [](const Api& api) {
        LOGI << "Api(http::verb::" << http::to_string(api.method) << ", \"" << api.name << "\"),";
    };
    LOGI << "active api:";
    for (const auto& item : kActiveApis) {
        out(item);
    }
    LOGI << "ignored api:";
    for (const auto& item: m_ignoredApis) {
        out(item);
    }
}
#endif

std::string LimiterGlobals::kLimiterServiceName = "LimiterService";

LimiterGlobals::ApiSet LimiterGlobals::kActiveApis = {
    Api(http::verb::patch, "/v2/contacts/filters"),
    Api(http::verb::put, "/v2/keys/signed"),
    Api(http::verb::put, "/v2/keys"),
    Api(http::verb::put, "/v2/group/deliver/update"),
    Api(http::verb::put, "/v2/group/deliver/invite"),
    Api(http::verb::put, "/v2/group/deliver/create"),
    Api(http::verb::put, "/v2/contacts/parts"),
    Api(http::verb::put, "/v2/contacts/friends/request"),
    Api(http::verb::put, "/v2/contacts/friends/reply"),
    Api(http::verb::put, "/v2/contacts/filters"),
    Api(http::verb::put, "/v1/profile/uploadGroupAvatar"),
    Api(http::verb::put, "/v1/profile/uploadAvatarPlaintext"),
    Api(http::verb::put, "/v1/profile/privacy"),
    Api(http::verb::put, "/v1/profile/nickname/:nickname"),
    Api(http::verb::put, "/v1/profile/namePlaintext/:name"),
    Api(http::verb::put, "/v1/profile/keys"),
    Api(http::verb::put, "/v1/profile/avatar"),
    Api(http::verb::put, "/v1/profile"),
    Api(http::verb::put, "/v1/messages/:uid"),
    Api(http::verb::put, "/v1/group/deliver/upload_password"),
    Api(http::verb::put, "/v1/group/deliver/update_user"),
    Api(http::verb::put, "/v1/group/deliver/update_notice"),
    Api(http::verb::put, "/v1/group/deliver/update"),
    Api(http::verb::put, "/v1/group/deliver/send_msg"),
    Api(http::verb::put, "/v1/group/deliver/review_join_request"),
    Api(http::verb::put, "/v1/group/deliver/recall_msg"),
    Api(http::verb::put, "/v1/group/deliver/query_member_list"),
    Api(http::verb::put, "/v1/group/deliver/query_member"),
    Api(http::verb::put, "/v1/group/deliver/query_last_mid"),
    Api(http::verb::put, "/v1/group/deliver/query_joined_list"),
    Api(http::verb::put, "/v1/group/deliver/query_info"),
    Api(http::verb::put, "/v1/group/deliver/leave"),
    Api(http::verb::put, "/v1/group/deliver/kick"),
    Api(http::verb::put, "/v1/group/deliver/join_group_by_code"),
    Api(http::verb::put, "/v1/group/deliver/invite"),
    Api(http::verb::put, "/v1/group/deliver/get_msg"),
    Api(http::verb::put, "/v1/group/deliver/create"),
    Api(http::verb::put, "/v1/group/deliver/ack_msg"),
    Api(http::verb::put, "/v1/attachments/upload/:attachmentId"),
    Api(http::verb::put, "/v1/attachments/upload"),
    Api(http::verb::put, "/v1/accounts/signup"),
    Api(http::verb::put, "/v1/accounts/signin"),
    Api(http::verb::put, "/v1/accounts/gcm"),
    Api(http::verb::put, "/v1/accounts/features"),
    Api(http::verb::put, "/v1/accounts/attributes"),
    Api(http::verb::put, "/v1/accounts/apn"),
    Api(http::verb::post, "/v2/contacts/parts"),
    Api(http::verb::post, "/v1/group/deliver/query_member_list_segment"),
    Api(http::verb::post, "/v1/group/deliver/query_info_batch"),
    Api(http::verb::post, "/v1/group/deliver/query_group_pending_list"),
    Api(http::verb::post, "/v1/group/deliver/member_list_ordered"),
    Api(http::verb::post, "/v1/group/deliver/is_qr_code_valid"),
    Api(http::verb::post, "/v1/group/deliver/get_owner_confirm"),
    Api(http::verb::post, "/v1/attachments/s3/upload_certification"),
    Api(http::verb::get, "/v2/keys/signed"),
    Api(http::verb::get, "/v2/keys/:uid/:device_id"),
    Api(http::verb::get, "/v2/keys"),
    Api(http::verb::get, "/v1/profile/version/:version"),
    Api(http::verb::get, "/v1/profile/keys"),
    Api(http::verb::get, "/v1/profile/download/:avatarId"),
    Api(http::verb::get, "/v1/profile/:uid"),
    Api(http::verb::get, "/v1/contacts"),
    Api(http::verb::get, "/v1/attachments/download/:attachmentId"),
    Api(http::verb::get, "/v1/attachments/:attachmentId"),
    Api(http::verb::get, "/v1/attachments"),
    Api(http::verb::get, "/v1/accounts/turn"),
    Api(http::verb::get, "/v1/accounts/challenge/:uid"),
    Api(http::verb::delete_, "/v2/contacts/friends"),
    Api(http::verb::delete_, "/v2/contacts/filters"),
    Api(http::verb::delete_, "/v1/accounts/gcm"),
    Api(http::verb::delete_, "/v1/accounts/apn"),
    Api(http::verb::delete_, "/v1/accounts/:uid/:signature"),
    Api(http::verb::put, "/v3/group/deliver/update"),
    Api(http::verb::put, "/v3/group/deliver/review_join_request"),
    Api(http::verb::put, "/v3/group/deliver/leave"),
    Api(http::verb::put, "/v3/group/deliver/kick"),
    Api(http::verb::put, "/v3/group/deliver/join_group_by_code"),
    Api(http::verb::put, "/v3/group/deliver/invite"),
    Api(http::verb::put, "/v3/group/deliver/group_keys_update"),
    Api(http::verb::put, "/v3/group/deliver/create"),
    Api(http::verb::put, "/v3/group/deliver/add_me"),
    Api(http::verb::put, "/v1/opaque_data"),
    Api(http::verb::post, "v3/group/deliver/dh_keys"),
    Api(http::verb::post, "/v3/group/deliver/prepare_key_update"),
    Api(http::verb::post, "/v3/group/deliver/members"),
    Api(http::verb::post, "/v3/group/deliver/latest_group_keys"),
    Api(http::verb::post, "/v3/group/deliver/group_keys"),
    Api(http::verb::post, "/v3/group/deliver/fire_group_keys_update"),
    Api(http::verb::get, "/v1/opaque_data/:index")
};
// api that are been ignored
// Api(http::verb::get, "/v1/accounts/sms/verification_code/:phonenumber"),
// Api(http::verb::get, "/v1/accounts/bind_phonenumber/:phonenumber/:verification_code"),
// Api(http::verb::get, "/v1/accounts/unbind_phonenumber"),
// Api(http::verb::put, "/v1/accounts/bind_phonenumber"),
// Api(http::verb::put, "/v1/contacts"),
// Api(http::verb::get, "/v1/contacts/token/:token"),
// Api(http::verb::put, "/v1/contacts/tokens"),
// Api(http::verb::put, "/v2/contacts/tokens"),
// Api(http::verb::get, "/v2/contacts/tokens/users"),
// Api(http::verb::post, "/v1/group/deliver/query_uids"),
// Api(http::verb::get, "/v1/keepalive/provisioning"),
// Api(http::verb::get, "/v1/keepalive")

}

