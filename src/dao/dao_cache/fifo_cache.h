#pragma once

#include "unordered_map"
#include "list"
#include "utils/log.h"

#include <boost/thread/shared_mutex.hpp>

#ifdef UNIT_TEST
#define private public
#define protected public
#endif

namespace bcm {
namespace dao {

template <class TKey, class TVal>
class FIFOCache {
public:
    FIFOCache(size_t limit) : m_limit(limit), m_size(0), m_bypass(false) {}

    bool set(const TKey& key, const TVal& value)
    {
        size_t size = value.ByteSizeLong();
        if (size > m_limit) {
            return false;
        }
        {
            boost::unique_lock<boost::shared_mutex> lock(m_mutex);
            // return if the cache is already existed
            if (m_caches.find(key) != m_caches.end()) {
                return true;
            }

            if (size + m_size > m_limit) {
                // drop the old caches to make available space for the new one
                auto it = m_sequence.begin();
                while (it != m_sequence.end()) {
                    auto cache = m_caches.find(**it);
                    if (cache == m_caches.end()) {
                        // clear cache while data corrupted
                        LOGE << "cache key: " << *it << " not exited in m_caches";
                        m_caches.clear();
                        m_sequence.clear();
                        m_size = 0;
                        break;
                    }
                    size_t s = cache->second.ByteSizeLong();
                    m_sequence.erase(it);
                    m_caches.erase(cache);
                    if (s > m_size) {
                        // clear cache while data corrupted
                        LOGE << "incorrect m_size for m_caches, cache entry size: " << m_caches.size();
                        m_caches.clear();
                        m_sequence.clear();
                        m_size = 0;
                    } else {
                        m_size -= s;
                    }
                    if (size + m_size <= m_limit) {
                        break;
                    }
                    it = m_sequence.begin();
                }
            }

            auto res = m_caches.emplace(key, value);
            m_sequence.push_back(&(res.first->first));
            m_size += size;
        }
        return true;
    }

    bool get(const TKey& key, TVal& value)
    {
        boost::shared_lock<boost::shared_mutex> lock(m_mutex);
        auto it = m_caches.find(key);
        if (it == m_caches.end()) {
            return false;
        }
        value = it->second;
        return true;
    }

private:
    size_t m_limit; // bytes limit
    size_t m_size; // current bytes size
    bool m_bypass;
    std::unordered_map<TKey, TVal> m_caches;
    std::list<const TKey*> m_sequence; // fifo sequential list
    boost::shared_mutex m_mutex;
};

}
}

#ifdef UNIT_TEST
#undef private
#undef protected
#endif
