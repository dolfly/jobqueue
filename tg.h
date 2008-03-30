#ifndef _JOBQUEUE_TGPARSE_H_
#define _JOBQUEUE_TGPARSE_H_

#include <stdio.h>

#include "agl/directedgraph.h"
#include "queue.h"

struct tgnode {
	char *name;
	char *cmd;
	double cost;
};

struct tgedge {
	char *src;
	char *dst;
	double cost;
};

struct tgjobs {
	size_t ngraphs;
	size_t njobs;

	struct dgraph *tg;
};

void tg_parse_jobfile(struct jobqueue *queue, char *jobfilename);

#endif
