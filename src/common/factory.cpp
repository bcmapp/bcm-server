#include "factory.h"

namespace bcm {

std::map<size_t, std::function<void*()>> Factory::kCreateFunctions;

}    // namespace bcm
