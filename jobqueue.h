#ifndef _JOBQUEUE_H_
#define _JOBQUEUE_H_

#include <stdio.h>
#include "vplist.h"

extern int passexecutionplace;
extern struct vplist machinelist;
extern int verbosemode;

FILE *get_next_jobfile(void);

#endif
