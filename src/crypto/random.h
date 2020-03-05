#pragma once

#include <random>

namespace bcm {

template <typename IntType,
          typename = typename std::enable_if<std::is_integral<IntType>::value, IntType>::type>
class SecureRandom {
public:
    static IntType next(IntType bound = 0) {
        static thread_local std::random_device rd;
        if (bound == 0) {
            std::uniform_int_distribution<IntType> dis;
            return dis(rd);
        } else if (bound > 0) {
            std::uniform_int_distribution<IntType> dis(0, bound);
            return dis(rd);
        } else {
            std::uniform_int_distribution<IntType> dis(bound);
            return dis(rd);
        }
    }
};

}