#ifndef _JOBQUEUE_QUEUE_H_
#define _JOBQUEUE_QUEUE_H_

#include <stdio.h>

struct jobqueue;

struct jobqueue {
	int (*next)(char *cmd, size_t maxlen, struct jobqueue *queue);

	void *data;
};

struct jobqueue *cq_init(char *argv[], int i, int argc);

#endif
