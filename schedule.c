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


enum job_result {
	JOB_SUCCESS,
	JOB_FAILURE
};

struct job_ack {
	int place;
	enum job_result result;
};

struct executionplace {
	size_t jobnumber;
	int busy;
};


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


static int read_job_ack(int fd, struct executionplace *places, int nplaces)
{
	struct job_ack joback;
	ssize_t ret;
	struct executionplace *place;

	ret = read(fd, &joback, sizeof joback);
	if (ret < 0) {
		if (errno == EAGAIN || errno == EINTR)
			return 0;

		die("read %zd: %s)\n", ret, strerror(errno));
	} else if (ret == 0) {
		die("Job queue pipe was broken\n");
	}

	if (ret != sizeof(joback))
		die("Unaligned read: returned %zd\n", ret);

	assert(joback.place < nplaces);

	place = &places[joback.place];

	place->busy = 0;

	if (verbosemode)
		fprintf(stderr, "Job %zd finished %s\n", place->jobnumber,
			joback.result == JOB_SUCCESS ?
			"successfully" : "unsuccessfully");

	return 1;
}


static void write_job_ack(int fd, struct job_ack joback, const char *cmd)
{
	ssize_t ret;

	/* pipe(7) guarantees write atomicity when sizeof(ps) <= PIPE_BUF */
	assert(sizeof(joback) <= PIPE_BUF);

	ret = write(fd, &joback, sizeof joback);
	if (ret < 0)
		die("PS %d job ack failed: %s\n", joback.place, cmd);

	if (ret != sizeof(joback))
		die("Unaligned write: returned %zd\n", ret);
}


static void run(const char *cmd, int ps, size_t jobnumber, int fd)
{
	ssize_t ret;
	char run_cmd[MAX_CMD_SIZE];
	struct job_ack joback = {.place = ps,
	                         .result = JOB_FAILURE};

	if (passexecutionplace) {
		ret = snprintf(run_cmd, sizeof run_cmd, "%s %d", cmd, ps + 1);
	} else {
		ret = snprintf(run_cmd, sizeof run_cmd, "%s", cmd);
	}

	if (ret >= sizeof(run_cmd)) {
		write_job_ack(fd, joback, run_cmd);
		die("Too long a command: %s\n", cmd); /* cmd, not run_cmd */
	}

	if (verbosemode)
		fprintf(stderr, "Job %zd execute: %s\n", jobnumber, run_cmd);

	ret = system(run_cmd);
	if (ret == -1) {
		write_job_ack(fd, joback, run_cmd);
		die("job delivery failed: %s\n", run_cmd);
	}

	joback.result = JOB_SUCCESS;

	write_job_ack(fd, joback, run_cmd);
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


void schedule(int nplaces)
{
	char cmd[MAX_CMD_SIZE];
	struct executionplace *places;
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

	assert(nplaces > 0);

	if (pipe_closeonexec(p))
		die("Can not create a pipe: %s\n", strerror(errno));

	/* All processing stations are non-busy in the beginning -> calloc() */
	places = calloc(nplaces, sizeof(places[0]));
	if (places == NULL)
		die("No memory for process array\n");

	while (1) {
		/* Find a free execution place */
		for (pindex = 0; pindex < nplaces; pindex++) {
			if (!places[pindex].busy)
				break;
		}

		if (pindex == nplaces || (exitmode && jobsdone < jobsread)) {

			if (read_job_ack(p[0], places, nplaces))
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

		places[pindex].busy = 1;
		places[pindex].jobnumber = jobsread;

		jobsread++;

		if (polling_fork() == 0) {
			/* Close some child file descriptors */
			close(0);
			close(p[0]);

			run(cmd, pindex, places[pindex].jobnumber, p[1]);

			exit(0);
		}
	}

	fprintf(stderr, "All jobs done (%zd)\n", jobsdone);
}
