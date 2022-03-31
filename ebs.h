#ifndef __EBS_H
#define __EBS_H

#include "client_status_defs.h"

// Do it this way to make a C/C++ library
#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#include <sys/types.h>

EXTERNC int ebs_init(char*, char*, char*, char*);

// Both read and write use fixed 4096 byte buffer sizes
EXTERNC int ebs_read(void*, off_t, bool crash_server = false);
EXTERNC int ebs_write(void*, off_t);

#endif
