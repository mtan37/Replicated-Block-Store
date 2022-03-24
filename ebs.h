#ifndef __EBS_H
#define __EBS_H

// Do it this way to make a C/C++ library
#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#include <sys/types.h>

EXTERNC int ebs_init();

// Both read and write use fixed 4096 byte buffer sizes
EXTERNC int ebs_read(void*, off_t);
EXTERNC int ebs_write(void*, off_t);

#endif
