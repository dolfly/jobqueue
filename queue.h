#ifndef _JOBQUEUE_QUEUE_H_
#define _JOBQUEUE_QUEUE_H_

#include <stdio.h>

#define MAX_CMD_SIZE 65536

struct jobqueue;

struct jobqueue {
	int (*next)(char *cmd, size_t maxlen, struct jobqueue *queue);

	void *data;
};

struct jobqueue *init_queue(char *argv[], int i, int argc, int taskgraphmode);

#endif
