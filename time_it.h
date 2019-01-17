#pragma once

#include <sys/time.h>

static inline long time_it(void) {
    struct timeval ts;
    gettimeofday(&ts, NULL);
    return ts.tv_sec * (int)1e6 + ts.tv_usec;
}
