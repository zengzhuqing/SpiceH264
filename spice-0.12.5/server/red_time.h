#ifndef H_RED_TIME
#define H_RED_TIME

#include <time.h>

static inline uint64_t red_now(void)
{
    struct timespec time;

    clock_gettime(CLOCK_MONOTONIC, &time);

    return ((uint64_t) time.tv_sec) * 1000000000 + time.tv_nsec;
}

#endif
