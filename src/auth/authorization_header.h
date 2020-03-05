#pragma once

#include <boost/optional.hpp>

namespace bcm {

class AuthorizationHeader {
public:
    AuthorizationHeader(const std::string& uid, const std::string& token, int deviceId);

    static boost::optional<AuthorizationHeader> parse(const std::string& header);
    static boost::optional<AuthorizationHeader> parse(const std::string& uidAndDevice
            , const std::string& token);

    const std::string& uid() const { return m_uid; }
    const std::string& token() const { return m_token; }
    int deviceId() const { return m_deviceId; }

private:
    std::string m_uid;
    std::string m_token;
    int m_deviceId;
};

}
