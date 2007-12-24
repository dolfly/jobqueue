#ifndef _JOBQUEUE_H_
#define _JOBQUEUE_H_

#include <stdio.h>
#include "vplist.h"

#define die(fmt, args...) do { \
    fprintf(stderr, fmt, ## args); \
    exit(1); \
  } while (0)

extern int executionplace;
extern int verbosemode;

FILE *get_next_jobfile(void);

#endif
