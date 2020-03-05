#pragma once
#include <string>
#include <vector>

namespace bcm {

class BaseFeatures {
public:
    BaseFeatures() = delete;
    BaseFeatures(uint32_t size);
    BaseFeatures(const BaseFeatures& rhs);
    BaseFeatures(BaseFeatures&& rhs) noexcept(std::is_nothrow_move_constructible<std::vector<uint8_t>>::value);
    BaseFeatures(const std::string& features);
    virtual ~BaseFeatures() = default;

    BaseFeatures& operator=(const BaseFeatures& rhs);
    BaseFeatures& operator=(BaseFeatures&& rhs) noexcept(std::is_nothrow_move_assignable<std::vector<uint8_t>>::value);

    bool hasFeature(uint32_t feature) const;
    BaseFeatures& addFeature(uint32_t feature);
    BaseFeatures& removeFeature(uint32_t feature);
    std::string getFeatures() const;

private:
    std::vector<uint8_t> m_features;
};

} // namespace bcm
