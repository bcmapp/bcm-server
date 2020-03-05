//
// Created by catror on 18-10-11.
//

#include "asio_yield.h"

namespace boost {
namespace fibers {
namespace asio {

thread_local yield_t yield{};

}
}
}