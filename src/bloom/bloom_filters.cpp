#include "bloom_filters.h"

namespace bcm {

static constexpr uint32_t kMurmHash3SeedRoot = 0xFBA4C795;
static constexpr uint32_t kMurmHash3Tweak = 0x3;
static constexpr uint32_t kMurmHash3HashNum = 0x5;

BloomFilters::BloomFilters(uint32_t algo, uint32_t length):
      m_full(false), m_empty(true), m_filters((length + 7)/8, 0)
{
    if (algo == 0) {
        m_hashFunc = std::make_shared<MurmurHash3Algo>(kMurmHash3HashNum, kMurmHash3SeedRoot, kMurmHash3Tweak);
    } else {
        m_hashFunc = std::make_shared<HashAlgo>(0);
    }
}

BloomFilters::BloomFilters(uint32_t algo, const std::string& content):
      BloomFilters(algo, content.size() * 8)
{
    m_filters.assign(content.begin(), content.end());
    updateEmptyFull();
}

bool BloomFilters::fromFiltersContent(const std::string& content)
{
    if (content.size() != m_filters.size()) {
        return false;
    }
    m_filters.assign(content.begin(), content.end());
    updateEmptyFull();
    return true;
}

void BloomFilters::insert(const std::string& key)
{
    if (m_full) {
        return;
    }

    auto hashes = m_hashFunc->getHashes(key);

    if (hashes.empty()) {
        return;
    }

    uint32_t index = 0;
    for (const auto& i : hashes) {
        index = i % (m_filters.size() * 8);
        m_filters[index >> 3] |= (1 << (7 & index));
    }

    m_empty = false;
}

bool BloomFilters::contains(const std::string& key) const
{
    if (m_full) {
        return true;
    }

    if (m_empty) {
        return false;
    }

    auto hashes = m_hashFunc->getHashes(key);

    if (hashes.empty()) {
        return false;
    }

    uint32_t index = 0;
    for (const auto& i : hashes) {
        index = i % (m_filters.size() * 8);
        if (!(m_filters[index >> 3] & (1 << (7 & index)))) {
            return false;
        }
    }

    return true;
}

void BloomFilters::updateEmptyFull()
{
    bool full = true;
    bool empty = true;
    for (const auto& i : m_filters)
    {
        full &= (i == 0xff);
        empty &= (i == 0);
    }
    m_full = full;
    m_empty = empty;
}

void BloomFilters::update(const std::map<uint32_t,bool>& values)
{
    for (const auto& m : values) {
        if (m.first < (m_filters.size() * 8)) {
            if (m.second) {
                m_filters[m.first >> 3] |= (1 << (7 & m.first));
            } else {
                m_filters[m.first >> 3] &= ~(1 << (7 & m.first));
            }
        }
    }
}

std::string BloomFilters::getFiltersContent() const
{
    std::string result;
    result.assign(m_filters.begin(), m_filters.end());
    return result;
}

} // namespace bcm
