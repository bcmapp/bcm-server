#pragma once

#include <dao/client.h>

namespace bcm {

class KeysManager {
public:
    bool get(const std::string& uid, boost::optional<uint32_t> deviceId, std::vector<OnetimeKey>& keys);
    bool getCount(const std::string& uid, uint32_t deviceId, uint32_t& count);
    bool set(const std::string& uid, uint32_t deviceId,
             const std::vector<OnetimeKey>& keys,
             const std::string& identityKey,
             const bcm::SignedPreKey& signedPreKey);
    bool clear(const std::string& uid, boost::optional<uint32_t> deviceId = boost::none);

private:
    std::shared_ptr<dao::OnetimeKeys> m_onetimeKeys {dao::ClientFactory::onetimeKeys()};
};

}

