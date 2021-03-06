#ifndef _JOBQUEUE_H_
#define _JOBQUEUE_H_

#include <stdio.h>
#include "vplist.h"

struct machine {
	char *name;
	int maxissue;
};

#define VERBOSE (verbosemode > 0)

extern struct vplist machinelist;
extern int maxissue;
extern int requeuefailedjobs;
extern int passexecutionplace;
extern int verbosemode;
extern size_t compute_eta_jobs;

#endif
