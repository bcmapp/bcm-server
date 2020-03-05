#pragma once

#include <string>
#include <boost/optional.hpp>

namespace bcm {

class DispatchAddress {
public:
    DispatchAddress(std::string uid, uint32_t deviceid);
    DispatchAddress(const DispatchAddress&) = default;
    DispatchAddress(DispatchAddress&&) = default;
    ~DispatchAddress() = default;

    DispatchAddress& operator=(const DispatchAddress&) = default;
    DispatchAddress& operator=(DispatchAddress&&) = default;

    const std::string& getUid() const { return m_uid; }
    uint32_t getDeviceid() const { return m_deviceid; }

    const std::string getSerialized() const {
        return m_uid + ":" + std::to_string(m_deviceid);
    }

    const std::string getSerializedForOnlineNotify() const {
        return "on:" + m_uid + ":" + std::to_string(m_deviceid);
    }

    static boost::optional<DispatchAddress> deserialize(const std::string& serialized);

private:
    std::string m_uid;
    uint32_t m_deviceid{0};
};

inline bool operator<(const DispatchAddress& lhs, const DispatchAddress& rhs)
{
    return lhs.getSerialized() < rhs.getSerialized();
}

inline std::ostream& operator<< (std::ostream& os, const DispatchAddress& address)
{
    os << address.getSerialized();
    return os;
}

}
