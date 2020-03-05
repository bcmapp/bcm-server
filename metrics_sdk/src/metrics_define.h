#pragma once

#include "metrics_log_utils.h"
#include <stdlib.h>

#define METRICS_ASSERT(x, LOG) do { \
        if (!(x)) { \
            METRICS_LOG_ERR(#LOG); \
            exit(0); \
        } \
    } while (0)
