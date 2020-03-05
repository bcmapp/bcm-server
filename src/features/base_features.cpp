#include "base_features.h"

namespace bcm {
BaseFeatures::BaseFeatures(uint32_t size) : m_features((size + 7) / 8, 0)
{
}

BaseFeatures::BaseFeatures(const BaseFeatures& rhs) : m_features(rhs.m_features)
{
}

BaseFeatures::BaseFeatures(BaseFeatures&& rhs) noexcept(std::is_nothrow_move_constructible<std::vector<uint8_t> >::value)
    : m_features(std::move(rhs.m_features))
{
}

BaseFeatures::BaseFeatures(const std::string& features) : m_features(features.begin(), features.end())
{
}

BaseFeatures& BaseFeatures::operator=(const BaseFeatures& rhs)
{
    m_features = rhs.m_features;
    return *this;
}

BaseFeatures& BaseFeatures::operator=(BaseFeatures&& rhs) noexcept(std::is_nothrow_move_assignable<std::vector<uint8_t> >::value)
{
    m_features = std::move(rhs.m_features);
    return *this;
}


bool BaseFeatures::hasFeature(uint32_t feature) const
{
    if (feature < (m_features.size() * 8)) {
        return m_features[feature >> 3] & (1 << (7 & feature));
    } else {
        return false;
    }
}

BaseFeatures& BaseFeatures::addFeature(uint32_t feature)
{
    if (feature < (m_features.size() * 8)) {
            m_features[feature >> 3] |= (1 << (7 & feature));
    }
    return *this;
}

BaseFeatures& BaseFeatures::removeFeature(uint32_t feature)
{
    if (feature < (m_features.size() * 8)) {
            m_features[feature >> 3] &= ~(1 << (7 & feature));
    }
    return *this;
}

std::string BaseFeatures::getFeatures() const
{
    std::string result;
    result.assign(m_features.begin(), m_features.end());
    return result;
}

} // namespace bcm
