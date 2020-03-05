#include "bcm_features.h"
#include "nlohmann/json.hpp"

namespace bcm {
BcmFeatures::BcmFeatures(uint32_t size) : m_features(size)
{
}

BcmFeatures::BcmFeatures(const BcmFeatures& rhs) : m_features(rhs.m_features)
{
}

BcmFeatures::BcmFeatures(BcmFeatures&& rhs) noexcept(std::is_nothrow_move_constructible<BaseFeatures>::value)
    : m_features(std::move(rhs.m_features))
{
}

BcmFeatures::BcmFeatures(const std::string& features) : m_features(features)
{
}

BcmFeatures& BcmFeatures::operator=(const BcmFeatures& rhs)
{
    m_features = rhs.m_features;
    return *this;
}

BcmFeatures& BcmFeatures::operator=(BcmFeatures&& rhs) noexcept(std::is_nothrow_move_constructible<BaseFeatures>::value)
{
    m_features = std::move(rhs.m_features);
    return *this;
}

bool BcmFeatures::hasFeature(Feature feature) const
{
    return m_features.hasFeature(static_cast<uint32_t>(feature));
}

BcmFeatures& BcmFeatures::addFeature(Feature feature)
{
    m_features.addFeature(static_cast<uint32_t>(feature));
    return *this;
}

BcmFeatures& BcmFeatures::removeFeature(Feature feature)
{
    m_features.removeFeature(static_cast<uint32_t>(feature));
    return *this;
}

std::string BcmFeatures::getFeatures() const
{
    return m_features.getFeatures();
}

std::vector<std::string> BcmFeatures::getFeatureNames() const
{
    std::vector<std::string> results;
    auto features = m_features.getFeatures();
    for (size_t i = 0;i < features.size(); ++i) {
        for (size_t j = 0; j < 8; ++j) {
            if ((features[i] & (1 << j)) && Feature_IsValid(i * 8 + j)) {
                results.push_back(Feature_Name(static_cast<Feature>(i * 8 + j)));
            }
        }
    }
    return results;
}

std::string BcmFeatures::getFeatureNamesJson() const
{
    nlohmann::json names(getFeatureNames());
    return names.dump();
}

} // namespace bcm
