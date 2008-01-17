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
	JOB_FAILURE,
	JOB_BROKEN_EXECUTION_PLACE
};

struct job {
	size_t jobnumber;
	char *cmd;
};

struct job_ack {
	size_t jobnumber;
	int place;
	enum job_result result;
};

struct executionplace {
	int jobsrunning;
	int broken;
};


static struct vplist failedjobs = VPLIST_INITIALIZER;

static struct vplist runningjobs = VPLIST_INITIALIZER;


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
	struct vplist *listitem;
	struct job *job;
	int found;
	char *machine;

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

	assert(place->jobsrunning > 0);

	if (!place->broken) {
		place->jobsrunning--;
	} else {

		if (machinelist.next != NULL) {
			machine = vplist_get(&machinelist, joback.place);
			assert(machine != NULL);
			fprintf(stderr, "Execution place %s ", machine);
		} else {
			fprintf(stderr, "Execution place %d ", joback.place);
		}

		fprintf(stderr, "is broken.\n"
			"Not issuing new jobs for that place.\n");

		/* The execution place is broken, prevent new jobs to it */
		place->jobsrunning = multiissue;
	}

	if (requeuefailedjobs && joback.result != JOB_SUCCESS) {
		found = 0;
		job = NULL;

		VPLIST_FOR_EACH(listitem, &runningjobs) {
			job = listitem->item;
			if (job->jobnumber == joback.jobnumber) {
				found = 1;
				break;
			}
		}

		assert(0); /* must remove job from runningjobs */

		assert(found);

		if (vplist_append(&failedjobs, job))
			die("Out of memory: can not requeue failed job\n");
	}

	if (verbosemode)
		fprintf(stderr, "Job %zd finished %s\n", joback.jobnumber,
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


static struct job *read_job(FILE **jobfile, size_t jobnumber)
{
	ssize_t ret;
	struct job *job;
	char cmd[MAX_CMD_SIZE];

	job = vplist_pop_head(&failedjobs);
	if (job != NULL) {
		if (vplist_append(&runningjobs, job))
			die("Out of memory: can not re-add job\n");

		return job;
	}

	while (1) {
		/* Read a new job and strip \n away from the command */
		ret = read_stripped_line(cmd, sizeof cmd, *jobfile);
		if (ret < 0) {
			fclose(*jobfile);

			*jobfile = get_next_jobfile();

			if (*jobfile == NULL)
				return NULL;

			continue;
		}

		if (useful_line(cmd))
			break;
	}

	job = malloc(sizeof job[0]);
	if (job == NULL)
		die("Can not allocate memory for job: %s\n", cmd);

	*job = (struct job) {.jobnumber = jobnumber,
			     .cmd = strdup(cmd)};

	if (job->cmd == NULL)
		die("Can not allocate memory for cmd: %s\n", cmd);

	vplist_append(&runningjobs, job);

	return job;
}


static void run(const struct job *job, int ps, int fd)
{
	ssize_t ret;
	char cmd[MAX_CMD_SIZE];
	struct job_ack joback = {.jobnumber = job->jobnumber,
				 .place = ps,
	                         .result = JOB_FAILURE};
	char *machine;

	if (machinelist.next != NULL) {

		machine = vplist_get(&machinelist, ps);
		assert(machine != NULL);

		ret = snprintf(cmd, sizeof cmd, "%s %s", job->cmd, machine);
	} else if (passexecutionplace) {
		ret = snprintf(cmd, sizeof cmd, "%s %d", job->cmd, ps + 1);
	} else {
		ret = snprintf(cmd, sizeof cmd, "%s", job->cmd);
	}

	if (ret >= sizeof(cmd)) {
		write_job_ack(fd, joback, cmd);
		die("Too long a command: %s\n", job->cmd);
	}

	if (verbosemode)
		fprintf(stderr, "Job %zd execute: %s\n", job->jobnumber, cmd);

	ret = system(cmd);
	if (ret == -1) {

		if (WEXITSTATUS(ret) == 2)
			joback.result = JOB_BROKEN_EXECUTION_PLACE;

		write_job_ack(fd, joback, cmd);
		die("job delivery failed: %s\n", cmd);
	}

	joback.result = JOB_SUCCESS;

	write_job_ack(fd, joback, cmd);
}


void schedule(int nplaces)
{
	struct executionplace *places;
	int pindex;
	int p[2];
	size_t jobsread = 0;
	size_t jobsdone = 0;
	int exitmode = 0;
	FILE *jobfile;
	struct job *job;
	int allbroken;

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
		allbroken = 1;

		for (pindex = 0; pindex < nplaces; pindex++) {
			if (!places[pindex].broken)
				allbroken = 0;

			if (places[pindex].jobsrunning < multiissue)
				break;
		}

		if (allbroken)
			die("SYSTEM FAILURE. All execution places dead.\n");

		if (pindex == nplaces || (exitmode && jobsdone < jobsread)) {

			if (read_job_ack(p[0], places, nplaces))
				jobsdone++;

			continue;
		}

		if (exitmode && jobsdone == jobsread)
			break;

		job = read_job(&jobfile, jobsread);
		if (job == NULL) {
			exitmode = 1; /* No more jobfiles -> exit */
			continue;
		}

		jobsread++;

		places[pindex].jobsrunning++;

		if (polling_fork() == 0) {
			/* Close some child file descriptors */
			close(0);
			close(p[0]);

			run(job, pindex, p[1]);

			exit(0);
		}
	}

	fprintf(stderr, "All jobs done (%zd)\n", jobsdone);
}
