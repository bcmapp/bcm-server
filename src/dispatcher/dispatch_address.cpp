#include "dispatch_address.h"
#include <vector>
#include <boost/algorithm/string.hpp>

namespace bcm {

DispatchAddress::DispatchAddress(std::string uid, uint32_t deviceid)
: m_uid(std::move(uid))
, m_deviceid(deviceid)
{
}

boost::optional<DispatchAddress> DispatchAddress::deserialize(const std::string& serialized)
{
    std::string uid;
    uint32_t deviceid{0};
    std::vector<std::string> uidAndDeviceId;
    boost::split(uidAndDeviceId, serialized, boost::is_any_of(":"));

    if (uidAndDeviceId.size() < 2) {
        return boost::none;
    }

    uid = uidAndDeviceId[0];

    try {
        deviceid = static_cast<uint32_t>(std::stoul(uidAndDeviceId[1]));
    } catch (std::exception& e) {
        return boost::none;
    }

    return DispatchAddress(uid, deviceid);
}

}

