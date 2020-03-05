#include "websocket_service.h"
#include <boost/system/error_code.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <fiber/asio_yield.h>
#include <utils/account_helper.h>
#include "redis/hiredis_client.h"
#include "redis/redis_manager.h"
#include "proto/device/multi_device.pb.h"

namespace bcm {

using namespace boost;
static const std::string kDeviceRequestLoginRedisPrefix = "DeviceRequest_";

WebsocketService::WebsocketService(std::string path,
                                   std::shared_ptr<ssl::context> sslCtx,
                                   std::shared_ptr<HttpRouter> router,
                                   std::shared_ptr<Authenticator> authenticator,
                                   std::shared_ptr<DispatchManager> dspatchManager,
                                   size_t concurrency,
                                   const std::shared_ptr<IValidator>& validator,
                                   WebsocketService::AuthType authType)
    : m_sslCtx(std::move(sslCtx))
    , m_router(std::move(router))
    , m_authenticator(std::move(authenticator))
    , m_dspatchManager(std::move(dspatchManager))
    , m_execPool(concurrency)
    , m_validator(validator)
    , m_authType(authType)
{
    if (path.back() == '/') {
        m_path = path.substr(0, path.size() - 1);
    } else {
        m_path  = path;
    }
}

void WebsocketService::run(std::string ip, uint16_t port)
{
    boost::ignore_unused(ip, port);
    m_execPool.run("websocket.worker");
    //TODO
}

WebsocketService::AuthResult WebsocketService::auth(const AuthRequest& authInfo)
{
    WebsocketService::AuthResult result;
    result.authCode = Authenticator::AUTHRESULT_UNKNOWN_ERROR;
    if (m_authType == TOKEN_AUTH) {
        auto authHeader = AuthorizationHeader::parse(authInfo.uid, authInfo.token);
        LOGD << "uid: " << authHeader->uid()
            << ", token: " << authHeader->token()
            << ", device id: " << authHeader->deviceId();
        if (authHeader) {
            Account account;
            auto client = AccountHelper::parseClientVersion(authInfo.clientVersion);
            result.authCode = m_authenticator->auth(authHeader.get(), client, account, Authenticator::AUTHTYPE_ALLOW_ALL);
            if (result.authCode == Authenticator::AUTHRESULT_SUCESS) {
                result.authEntity = account;
            } else if (result.authCode == Authenticator::AUTHRESULT_TOKEN_VERIFY_FAILED) {
                result.deviceName = AccountsManager::getAuthDeviceName(account);
            } else {
                // nothing todo
            }
        } else {
            result.authCode = Authenticator::AUTHRESULT_AUTH_HEADER_ERROR;
            LOGD << "cannot find login params";
        }
    } else if (m_authType == REQUESTID_AUTH) {
        if (authInfo.requestId.empty()) {
            result.authCode = Authenticator::AUTHRESULT_REQUESTID_NOT_FOUND;
            return result;
        }
        std::string info;
        std::string redisKey = kDeviceRequestLoginRedisPrefix + authInfo.requestId;
        bool getRes = RedisDbManager::Instance()->get(redisKey, info);

        if (!getRes || info.empty()) {
            result.authCode = Authenticator::AUTHRESULT_REQUESTID_NOT_FOUND;
        } else {
            DeviceLoginReqInfo reqInfo;
            if (!reqInfo.ParseFromString(info)) {
                result.authCode = Authenticator::AUTHRESULT_REQUESTID_NOT_FOUND;
            } else {
                result.authCode = Authenticator::AUTHRESULT_SUCESS;
                result.authEntity = authInfo.requestId;
            }
        }
    } else {
        result.authCode = Authenticator::AUTHRESULT_UNKNOWN_ERROR;
    }
    return result;
}

bool WebsocketService::match(const std::string& path)
{
    size_t compareSize = path.size();
    if (path.back() == '/') {
        compareSize -= 1;
    }

    return !path.compare(0, compareSize, m_path);
}

void WebsocketService::stop()
{
    m_execPool.stop();
    //TODO
}

void WebsocketService::loop(std::shared_ptr<boost::asio::io_context> ioc, std::string& ip, uint16_t port)
{
    boost::ignore_unused(ioc, ip, port);
    //TODO
}


}

