#ifndef __HELPER_H
#define __HELPER_H

void set_time(struct timespec* ts);
double difftimespec_s(const struct timespec before, const struct timespec after);
double difftimespec_ns(const struct timespec before, const struct timespec after);
#endif