#include "keys_manager.h"
#include <utils/log.h>

namespace bcm {

bool KeysManager::get(const std::string& uid, boost::optional<uint32_t> deviceId, std::vector<OnetimeKey>& keys)
{
    dao::ErrorCode error;
    if (deviceId) {
        OnetimeKey key;
        error = m_onetimeKeys->get(uid, *deviceId, key);
        if (error == dao::ERRORCODE_SUCCESS) {
            keys.push_back(key);
        }
    } else {
        error = m_onetimeKeys->get(uid, keys);
    }

    if ((error != dao::ERRORCODE_SUCCESS) && (error != dao::ERRORCODE_NO_SUCH_DATA)) {
        LOGE << "get pre key failed, for " << uid << "." << deviceId.get_value_or(0) << ", error: " << error;
        return false;
    }

    return true;
}

bool KeysManager::getCount(const std::string& uid, uint32_t deviceId, uint32_t& count)
{
    dao::ErrorCode error = m_onetimeKeys->getCount(uid, deviceId, count);
    if (error == dao::ERRORCODE_NO_SUCH_DATA) {
        count = 0;
        return true;
    }

    if (error != dao::ERRORCODE_SUCCESS) {
        LOGE << "get keys count failed, for " << uid << "." << deviceId << ", error: " << error;
        return false;
    }

    return true;
}

bool KeysManager::set(const std::string& uid, uint32_t deviceId,
                      const std::vector<bcm::OnetimeKey>& keys,
                      const std::string& identityKey,
                      const bcm::SignedPreKey& signedPreKey)
{
    dao::ErrorCode error = m_onetimeKeys->set(uid, deviceId, keys, identityKey, signedPreKey);
    if (error != dao::ERRORCODE_SUCCESS) {
        LOGE << "store pre keys failed, for " << uid << "." << deviceId << ", error: " << error;
        return false;
    }
    return true;
}

bool KeysManager::clear(const std::string& uid, boost::optional<uint32_t> deviceId)
{
    dao::ErrorCode error;
    if (deviceId) {
        error = m_onetimeKeys->clear(uid, *deviceId);
    } else {
        error = m_onetimeKeys->clear(uid);
    }

    if (error != dao::ERRORCODE_SUCCESS) {
        LOGE << "clear keys failed, for " << uid << "." << deviceId.get_value_or(0) << ", error: " << error;
        return false;
    }

    return true;
}

}