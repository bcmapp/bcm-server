#pragma once

#include <string>
#include <vector>

namespace bcm {

enum HashAlgoNum {

};

class HashAlgo {
public:
    HashAlgo(uint32_t hashNum) : m_hashNum(hashNum)
    {
    }

    virtual ~HashAlgo()
    {
    }

    virtual std::vector<uint32_t> getHashes(const std::string&)
    {
        return std::vector<uint32_t>();
    }

protected:
    uint32_t m_hashNum;
};

class MurmurHash3Algo : public HashAlgo {
public:
    MurmurHash3Algo(uint32_t hashNum, uint32_t rootSeed, uint32_t tweak);

    virtual ~MurmurHash3Algo()
    {
    }

    virtual std::vector<uint32_t> getHashes(const std::string& key) override;

private:
    uint32_t m_tweak = 0;
    uint32_t m_rootSeed = 0;
};

}
