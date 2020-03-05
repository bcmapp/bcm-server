#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "hash_algo.h"

namespace bcm {

class BloomFilters {
    public:
        BloomFilters(uint32_t algo, uint32_t length);
        BloomFilters(uint32_t algo, const std::string& content);
        ~BloomFilters(){}
        bool fromFiltersContent(const std::string& content);
        void insert(const std::string& key);
        bool contains(const std::string& key) const;
        void update(const std::map<uint32_t,bool>& values);
        std::string getFiltersContent() const;

    private:
        void updateEmptyFull();

    private:
        bool m_full = false;
        bool m_empty = true;
        std::vector<uint8_t> m_filters;
        std::shared_ptr<HashAlgo> m_hashFunc = nullptr;
};

} // namespace bcm
