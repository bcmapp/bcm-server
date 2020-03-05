#pragma once

#include <string>
#include "proto/dao/onetime_key.pb.h"
#include "proto/dao/error_code.pb.h"
#include "../proto/dao/pre_key.pb.h"

namespace bcm {
namespace dao {

/*
 * OnetimeKeys dao virtual base class
 */
class OnetimeKeys {
public:
    /*
     * return ERRORCODE_SUCCESS if get onetimeKeys success
     * attention,this op should get the first onetimeKey sort by deviceId,keyId
     * and atomicly del the data when get
     */
    virtual ErrorCode get(const std::string& uid, std::vector<bcm::OnetimeKey>& keys) = 0;

    /*
     * return ERRORCODE_SUCCESS if get onetimeKeys success
     * attention,this op should get the first onetimeKey sort by keyId
     * and atomicly del the data when get
     */
    virtual ErrorCode get(const std::string& uid, uint32_t deviceId, bcm::OnetimeKey& key) = 0;

    /*
     * return ERRORCODE_SUCCESS if get onetimeKeys count success
     */
    virtual ErrorCode getCount(const std::string& uid, uint32_t deviceId, uint32_t& count) = 0;

    /*
     * return ERRORCODE_SUCCESS if set onetimeKeys success
     * attention,before set,should del all keys first
     */
    virtual ErrorCode set(const std::string& uid, uint32_t deviceId,
                          const std::vector <bcm::OnetimeKey>& keys,
                          const std::string& identityKey,
                          const bcm::SignedPreKey& signedPreKey) = 0;

    /*
     * return ERRORCODE_SUCCESS if clear onetimeKeys success
     */
    virtual ErrorCode clear(const std::string& uid) = 0;

    /*
     * return ERRORCODE_SUCCESS if clear onetimeKeys success
     */
    virtual ErrorCode clear(const std::string& uid, uint32_t deviceId) = 0;
};

}  // namespace dao
}  // namespace bcm
