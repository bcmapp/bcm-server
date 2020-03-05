#pragma once

#include "proto/dao/error_code.pb.h"

namespace bcm {
namespace dao {

class OpaqueData {
public:
    /*
     * return ERRORCODE_SUCCESS if set opaque data success
     */
    virtual ErrorCode setOpaque(
            const std::string& key,
            const std::string& value) = 0;

    /*
     * return ERRORCODE_SUCCESS if get opaque data success
     */
    virtual ErrorCode getOpaque(
            const std::string& key,
            std::string& value) = 0;
};

}
}
