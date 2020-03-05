#include "authorization_header.h"
#include <boost/algorithm/string.hpp>
#include <crypto/base64.h>
#include <proto/dao/device.pb.h>
#include <utils/log.h>

namespace bcm {

using namespace boost;

AuthorizationHeader::AuthorizationHeader(const std::string& uid, const std::string& token, int deviceId)
    : m_uid(uid)
    , m_token(token)
    , m_deviceId(deviceId)
{

}

boost::optional<AuthorizationHeader> AuthorizationHeader::parse(const std::string& uidAndDevice
        , const std::string& token)
{
    std::vector<std::string> uidAndDeviceId;
    boost::split(uidAndDeviceId, uidAndDevice, boost::is_any_of("."));
    int deviceId = Device::MASTER_ID;
    if (uidAndDeviceId.size() > 1) {
        try {
            deviceId = std::stoi(uidAndDeviceId[1]);
        } catch (std::exception&) {
            LOGT << "device id is not an integer: " << uidAndDeviceId[1];
        }
    }
    return AuthorizationHeader(uidAndDeviceId[0], token, deviceId);
}

boost::optional<AuthorizationHeader> AuthorizationHeader::parse(const std::string& header)
{
    std::vector<std::string> headerParts;
    boost::split(headerParts, header, boost::is_any_of(" "));

    if (headerParts.size() < 2) {
        return boost::none;
    }

    if (headerParts[0] != "Basic") {
        return boost::none;
    }

    std::string concatenated = Base64::decode(headerParts[1]);
    if (concatenated.empty()) {
        return boost::none;
    }

    std::vector<std::string> credentialParts;
    boost::split(credentialParts, concatenated, boost::is_any_of(":"));
    if (credentialParts.size() < 2) {
        return boost::none;
    }

    return parse(credentialParts[0], credentialParts[1]);
}

}
