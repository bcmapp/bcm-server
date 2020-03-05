#pragma once
#include "base_features.h"
#include "proto/features/features.pb.h"

namespace bcm {
class BcmFeatures {
public:
    BcmFeatures() = delete;
    BcmFeatures(uint32_t size);
    BcmFeatures(const BcmFeatures& rhs);
    BcmFeatures(BcmFeatures&& rhs) noexcept(std::is_nothrow_move_constructible<BaseFeatures>::value);
    BcmFeatures(const std::string& features);
    virtual ~BcmFeatures() = default;

    BcmFeatures& operator=(const BcmFeatures& rhs);
    BcmFeatures& operator=(BcmFeatures&& rhs) noexcept(std::is_nothrow_move_constructible<BaseFeatures>::value);

    bool hasFeature(Feature feature) const;
    BcmFeatures& addFeature(Feature feature);
    BcmFeatures& removeFeature(Feature feature);
    std::string getFeatures() const;
    std::vector<std::string> getFeatureNames() const;
    std::string getFeatureNamesJson() const;
private:
    BaseFeatures m_features;
};
} // namespace bcm
