#include "hash_algo.h"
#include "crypto/murmurhash3.h"

namespace bcm {

MurmurHash3Algo::MurmurHash3Algo(uint32_t hashNum, uint32_t rootSeed, uint32_t tweak)
    : HashAlgo(hashNum), m_tweak(tweak), m_rootSeed(rootSeed)
{
}

std::vector<uint32_t> MurmurHash3Algo::getHashes(const std::string& key)
{
    std::vector<uint32_t> hashes(m_hashNum);
    for (uint32_t i = 0; i < m_hashNum; ++i) {
        hashes[i] = MurmurHash3::murmurHash3(i * m_rootSeed + m_tweak, key);
    }
    return hashes;
}

} // namespace bcm
