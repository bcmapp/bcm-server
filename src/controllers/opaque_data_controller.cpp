#include "opaque_data_controller.h"
#include <crypto/any_base.h>
#include <crypto/base64.h>
#include <crypto/murmurhash3.h>
#include <crypto/random.h>
#include <utils/log.h>

namespace bcm {

void OpaqueDataController::addRoutes(HttpRouter &router) {
    router.add(http::verb::put, "/v1/opaque_data", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&OpaqueDataController::setOpaqueData, shared_from_this(), std::placeholders::_1),
               new JsonSerializerImp<OpaqueContent>, new JsonSerializerImp<OpaqueIndex>);

// #ifndef NDEBUG
    router.add(http::verb::get, "/v1/opaque_data/:index", Authenticator::AUTHTYPE_ALLOW_MASTER,
               std::bind(&OpaqueDataController::getOpaqueData, shared_from_this(), std::placeholders::_1),
               nullptr, new JsonSerializerImp<OpaqueContent>);
// #endif
}

void OpaqueDataController::setOpaqueData(HttpContext &context) {
    static constexpr uint32_t kHashSeed = 0xFBA4C795;
    static constexpr int kIndexBase = 62;
    static constexpr int kMaxLength = 16 * 1024;

    auto &response = context.response;
    auto &content = boost::any_cast<OpaqueContent &>(context.requestEntity).content;

    if (!Base64::check(content) || content.length() > kMaxLength) {
        context.statics.setMessage("content length is " +
                                    std::to_string(content.length()));
        return response.result(http::status::bad_request);
    }

    uint32_t hash = MurmurHash3::murmurHash3(kHashSeed, content);

    uint64_t index = hash;
    std::string encodedIndex;

    dao::ErrorCode ec;
    int remainTimes = 10;
    do {
        AnyBase::encode(index, kIndexBase, encodedIndex);
        ec = m_opaqueData->setOpaque(encodedIndex, content);
        if (ec != dao::ERRORCODE_ALREADY_EXSITED) {
            break;
        }

        // if existed, fill the high 32 bt randomly
        index = hash | (SecureRandom<uint64_t>::next(UINT32_MAX) << 32U);
    } while ((--remainTimes) > 0);

    if (remainTimes <= 0) {
        LOGW << "too many collisions: " << hash << "," << content;
        return response.result(http::status::internal_server_error);
    }

    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGW << "store opaque data failed: " << ec;
        return response.result(http::status::internal_server_error);
    }

    context.responseEntity = OpaqueIndex{encodedIndex};
    return response.result(http::status::ok);
}

void OpaqueDataController::getOpaqueData(HttpContext &context) {
    auto &response = context.response;
    auto &index = context.pathParams[":index"];

    std::string content;
    auto ec = m_opaqueData->getOpaque(index, content);
    if (ec != dao::ERRORCODE_SUCCESS) {
        LOGW << "read opaque data failed: " << ec;
        return response.result(http::status::bad_request);
    }

    context.responseEntity = OpaqueContent{content};
    return response.result(http::status::ok);
}

} // namespace bcm
