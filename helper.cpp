#include <iostream>

#include "helper.h"

void set_time(struct timespec* ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

double difftimespec_s(const struct timespec before, const struct timespec after) {
    return ((double)after.tv_sec - (double)before.tv_sec);
}

double difftimespec_ns(const struct timespec before, const struct timespec after)
{
    return ((double)after.tv_sec - (double)before.tv_sec) * (double)1000000000
         + ((double)after.tv_nsec - (double)before.tv_nsec);
}
