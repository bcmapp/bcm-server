#include "any_base.h"
#include <vector>

namespace bcm {

static constexpr char kNumberSymbol[] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
    'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
    'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D',
    'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z'
};

static constexpr char kMaxBase = sizeof(kNumberSymbol) / sizeof(char);

bool AnyBase::encode(uint64_t num, int base, std::string& encoded)
{
    if (base > kMaxBase) {
        return false;
    }

    std::vector<char> vtmp;
    uint64_t remain;
    do {
        remain = num % base;
        num /= base;
        vtmp.emplace_back(kNumberSymbol[remain]);
    } while (num > 0);

    encoded.assign(vtmp.rbegin(), vtmp.rend());
    return true;
}

bool AnyBase::decode(const std::string& encoded, int base, uint64_t& num)
{
    if (base > kMaxBase) {
        return false;
    }

    num = 0;
    int tmp;
    for (const auto& c : encoded) {
        if ('0' <= c && c <= '9') {
            tmp = c - '0';
        } else if ('a' <= c && c <= 'z') {
            tmp = c - 'a' + 10;
        } else if ('A' <= c && c <= 'Z') {
            tmp = c - 'A' + 36;
        } else {
            return false;
        }
        num = num * base + tmp;
    }

    return true;
}

}
