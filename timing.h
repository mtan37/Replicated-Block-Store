#ifndef __TIMING_H
#define __TIMING_H
#include <stdio.h>
#include <math.h>
#include <time.h>

#define NANOS_PER_SEC (1000*1000*1000l)

#define DO_TRIALS(setup, task, trials, var) {                                  \
  var = NANOS_PER_SEC;                                                         \
  struct timespec _start, _end;                                                \
  unsigned long long _total_time = 0;                                               \
  for (int _i = 0; _i < trials; ++_i) {                                        \
    setup                                                                      \
    clock_gettime(CLOCK_MONOTONIC, &_start);                                   \
    task                                                                       \
    clock_gettime(CLOCK_MONOTONIC, &_end);                                     \
    unsigned long _nanos = nanos_diff(&_start, &_end);                         \
    _total_time += _nanos;                                                     \
  }                                                                            \
  var = _total_time/trials;                                                    \
}

static unsigned long nanos_diff(struct timespec* start, struct timespec* end) {
  unsigned long nanos = end->tv_nsec - start->tv_nsec;
  unsigned long secs = end->tv_sec - start->tv_sec;
  if (end->tv_nsec < start->tv_nsec) {
    nanos += NANOS_PER_SEC;
    secs -= 1;
  }
  return nanos + NANOS_PER_SEC * secs;
}

void print_result(char* name, double nanos) {
  if (nanos == INFINITY) {
    printf("%-35s %13s\n", name, "N/A");
    return;
  }
  printf("%-35s %10.2f ns", name, nanos);
  if (nanos > 1000) {
    printf("%10.2f us", nanos/1000);
    if (nanos > 1000000) {
      printf("%10.2f ms", nanos/1000000);
    }
  }
  printf("\n");
}
#endif //__TIMING_H

