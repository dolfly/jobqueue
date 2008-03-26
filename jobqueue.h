#ifndef _JOBQUEUE_H_
#define _JOBQUEUE_H_

#include <stdio.h>
#include "vplist.h"

#define VERBOSE (verbosemode > 0)

extern struct vplist machinelist;
extern int maxissue;
extern int requeuefailedjobs;
extern int passexecutionplace;
extern int verbosemode;

#endif
