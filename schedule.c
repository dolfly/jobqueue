#define _GNU_SOURCE

#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <math.h>
#include <assert.h>

#include "jobqueue.h"
#include "schedule.h"
#include "support.h"


#define MAX_CMD_SIZE 65536


static pid_t polling_fork(void)
{
	pid_t child;

	while (1) {
		child = fork();
		
		if (child >= 0)
			break;

		/* Can not fork(): sleep a bit and try again */
		sleep(1);
	}

	return child;
}


static void run(const char *cmd, int ps, int fd)
{
	ssize_t ret;
	char run_cmd[MAX_CMD_SIZE];

	if (executionplace) {
		ret = snprintf(run_cmd, sizeof run_cmd, "%s %d", cmd, ps + 1);
	} else {
		ret = snprintf(run_cmd, sizeof run_cmd, "%s", cmd);
	}

	if (ret >= sizeof(run_cmd))
		die("Too long a command: %s\n", cmd); /* cmd, not run_cmd */

	if (verbosemode)
		fprintf(stderr, "EXECUTE: %s\n", run_cmd);

	ret = system(run_cmd);
	if (ret == -1)
		die("job delivery failed: %s\n", run_cmd);

	/* pipe(7) guarantees write atomicity when sizeof(ps) <= PIPE_BUF */
	assert(sizeof(ps) <= PIPE_BUF);

	ret = write(fd, &ps, sizeof ps);
	if (ret < 0)
		die("PS %d job ack failed: %s\n", ps, run_cmd);

	assert(ret == sizeof(ps));
}


/* Read a line from file 'f' to buffer 'buf' with size 'buflen', and
 * strip \n away
 */
static ssize_t read_stripped_line(char *buf, size_t buflen, FILE *f)
{
	size_t len;

	while (fgets(buf, buflen, f) == NULL) {
		/* fgets() may be unable to restart after SIGCHLD:
		   it can return NULL, and feof() needs to be tested */
		if (feof(f))
			return -1;
	}

	len = strlen(buf);

	if (buf[len - 1] == '\n') {
		len--;
		buf[len] = 0;
	}

	return len;
}


void schedule(int nprocesses)
{
	char cmd[MAX_CMD_SIZE];
	char *busy;
	int pindex;
	int p[2];
	ssize_t ret;
	size_t jobsread = 0;
	size_t jobsdone = 0;
	int exitmode = 0;
	FILE *jobfile;

	jobfile = get_next_jobfile();
	if (jobfile == NULL)
		exitmode = 1; /* If no valid files, go to exitmode */

	assert(nprocesses > 0);

	if (pipe_closeonexec(p))
		die("Can not create a pipe: %s\n", strerror(errno));

	/* All processing stations are not busy in the beginning -> calloc() */
	busy = calloc(nprocesses, 1);
	if (busy == NULL)
		die("No memory for process array\n");

	while (1) {
		/* Find a free execution place */
		for (pindex = 0; pindex < nprocesses; pindex++) {
			if (!busy[pindex])
				break;
		}

		if (pindex == nprocesses ||
		    (exitmode && jobsdone < jobsread)) {

			ret = read(p[0], &pindex, sizeof pindex);
			if (ret < 0) {
				if (errno == EAGAIN || errno == EINTR)
					continue;

				die("read %zd: %s)\n", ret, strerror(errno));
			} else if (ret == 0) {
				die("Job queue pipe was broken\n");
			}

			if (ret != sizeof(pindex))
				die("Unaligned read: returned %zd\n", ret);

			/* Note: pindex has a new value from previous read() */
			busy[pindex] = 0;

			jobsdone++;

			continue;
		}

		if (exitmode && jobsdone == jobsread)
			break;

		/* Read a new job and strip \n away from the command */
		ret = read_stripped_line(cmd, sizeof cmd, jobfile);
		if (ret < 0) {

			fclose(jobfile);

			jobfile = get_next_jobfile();

			if (jobfile == NULL)
				exitmode = 1; /* No more jobfiles -> exit */

			continue;
		}

		/* Ignore empty lines and lines beginning with '#' */
		if (ret == 0 || cmd[0] == '#')
			continue;

		jobsread++;
		busy[pindex] = 1;

		if (polling_fork() == 0) {
			/* Close some child file descriptors */
			close(0);
			close(p[0]);

			run(cmd, pindex, p[1]);

			exit(0);
		}
	}

	fprintf(stderr, "All jobs done (%zd)\n", jobsdone);
}
