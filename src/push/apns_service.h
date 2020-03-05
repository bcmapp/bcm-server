#pragma once

#include <vector>
#include <string>
#include "config/apns_config.h"
#include "apns_client.h"

namespace bcm {
namespace push {
namespace apns {

class ServiceImpl;

class Service {
public:
    Service();
    ~Service();

    bool init(const ApnsConfig& apns) noexcept;
    const std::string& bundleId(const std::string& type, const bool isVoip = false) const noexcept;
    int32_t expirySecs() const noexcept;
    SendResult send(const std::string& type,
                    const Notification& notification) noexcept;

private:
    ServiceImpl* m_pImpl;
    ServiceImpl& m_impl;
};

} // namespace apns
} // namespace push
} // namespace bcm