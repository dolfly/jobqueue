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
	JOB_SUCCESS = 0,
	JOB_FAILURE,
	JOB_BROKEN_EXECUTION_PLACE,
	JOB_RESULT_MAXIMUM
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


static void read_job_ack(size_t *jobsdone, int fd,
			 struct executionplace *places, int nplaces)
{
	struct job_ack joback;
	ssize_t ret;
	struct executionplace *place;
	struct vplist *listnode;
	struct job *job;
	int found;
	char *machine;

	ret = read(fd, &joback, sizeof joback);
	if (ret < 0) {
		if (errno == EAGAIN || errno == EINTR)
			return;

		die("read %zd: %s)\n", ret, strerror(errno));
	} else if (ret == 0) {
		die("Job queue pipe was broken\n");
	}

	if (ret != sizeof(joback))
		die("Unaligned read: returned %zd\n", ret);

	assert(joback.place < nplaces);

	place = &places[joback.place];

	if (requeuefailedjobs && joback.result == JOB_BROKEN_EXECUTION_PLACE) {
		/* The execution place is broken, prevent new jobs to it */
		place->broken = 1;

		if (machinelist.next != NULL) {
			machine = vplist_get(&machinelist, joback.place);
			assert(machine != NULL);
			fprintf(stderr, "Execution place %s ", machine);
		} else {
			fprintf(stderr, "Execution place %d ", joback.place);
		}

		fprintf(stderr, "is broken.\n"
			"Not issuing new jobs for that place.\n");
	}

	assert(place->jobsrunning > 0);

	if (place->broken)
		place->jobsrunning = maxissue;
	else
		place->jobsrunning--;

	if (requeuefailedjobs) {

		if (joback.result == JOB_SUCCESS) {
			(*jobsdone)++;
		} else {
			found = 0;
			job = NULL;

			VPLIST_FOR_EACH(listnode, &runningjobs) {
				job = listnode->item;

				if (job->jobnumber == joback.jobnumber) {
					found = 1;
					break;
				}
			}

			assert(found);

			/* Remove the job from runningjobs, and
			 *  add it to failedjobs to restart it later */
			if (vplist_remove_item(&runningjobs, job))
				die("BUG: running jobs item not found\n");

			if (vplist_append(&failedjobs, job))
				die("Out of memory: can not requeue failed job\n");
		}
	} else {
		(*jobsdone)++;
	}

	if (VERBOSE)
		fprintf(stderr, "Job %zd finished %s\n", joback.jobnumber,
			joback.result == JOB_SUCCESS ?
			"successfully" : "unsuccessfully");
}


static void write_job_ack(int fd, struct job_ack joback, const char *cmd)
{
	ssize_t ret;

	/* pipe(7) guarantees write atomicity when size <= PIPE_BUF */
	assert(sizeof(joback) <= PIPE_BUF);

	ret = write(fd, &joback, sizeof joback);
	if (ret < 0)
		die("PS %d job ack failed: %s\n", joback.place, cmd);

	if (ret != sizeof(joback))
		die("Unaligned write: returned %zd\n", ret);
}


static struct job *read_job(FILE **jobfile, size_t *jobsread)
{
	ssize_t ret;
	struct job *job;
	char cmd[MAX_CMD_SIZE];

	job = vplist_pop_head(&failedjobs);
	if (job != NULL) {
		if (vplist_append(&runningjobs, job))
			die("Out of memory: can not re-add job\n");

		/* Do not increase *jobsread for restarted jobs */
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

	*job = (struct job) {.jobnumber = *jobsread,
			     .cmd = strdup(cmd)};

	if (job->cmd == NULL)
		die("Can not allocate memory for cmd: %s\n", cmd);

	if (vplist_append(&runningjobs, job))
		die("Can not append job to running list: %s\n", cmd);

	(*jobsread)++;

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

	if (!vplist_is_empty(&machinelist)) {

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

	if (VERBOSE)
		fprintf(stderr, "Job %zd execute: %s\n", job->jobnumber, cmd);

	ret = system(cmd);
	if (ret == -1) {
		write_job_ack(fd, joback, cmd);
		die("job delivery failed: %s\n", cmd);
	}

	ret = WEXITSTATUS(ret);

	if (ret < JOB_RESULT_MAXIMUM) {
		joback.result = ret;
	} else {
		/* Mark large return code as a failure */
		joback.result = JOB_FAILURE;

		if (requeuefailedjobs)
			fprintf(stderr, "Invalid return code %d from: %s\n"
				"Intepreting this as a failure.\n",
				(int) ret, cmd);
	}

	write_job_ack(fd, joback, cmd);
}


void schedule(int nplaces)
{
	struct executionplace *places;
	int pindex;
	int ackpipe[2];
	size_t jobsread = 0;
	size_t jobsdone = 0;
	int exitmode = 0;
	FILE *jobfile;
	struct job *job;
	int allbroken;
	pid_t child;
	int somethingtoissue;
	int possibletoissue;
	int somethingtowait;

	jobfile = get_next_jobfile();
	if (jobfile == NULL)
		exitmode = 1; /* If no valid files, go to exitmode */

	assert(nplaces > 0);

	if (pipe_closeonexec(ackpipe))
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

			if (places[pindex].jobsrunning < maxissue)
				break;
		}

		if (allbroken)
			die("ALL EXECUTION PLACES HAVE DIED\n");

		possibletoissue = (pindex < nplaces);

		somethingtoissue = (!vplist_is_empty(&failedjobs) || !exitmode);

		somethingtowait = (jobsdone < jobsread);

		/* Finite state machine for job handling
		 *
		 * PI = "possibletoissue", SI = "somethingtoissue"
		 * SW = "somethingtowait"
                 *
		 *  state | PI | SI | SW | Action
		 *  -----------------------------
		 *  0     | 0    0    0  | EXIT
		 *  1     | 0    0    1  | WAIT
		 *  2     | 0    1    0  | WAIT
		 *  3     | 0    1    1  | WAIT
		 *  4     | 1    0    0  | EXIT
		 *  5     | 1    0    1  | WAIT
		 *  6     | 1    1    0  | ISSUE
		 *  7     | 1    1    1  | ISSUE
		 */

		/* States 6 and 7 */
		if (possibletoissue && somethingtoissue) {
			job = read_job(&jobfile, &jobsread);
			if (job == NULL) {
				exitmode = 1; /* No more jobfiles -> exit */
				continue;
			}

			places[pindex].jobsrunning++;

			child = fork();
			if (child == 0) {
				/* Close some child file descriptors */
				close(0);
				close(ackpipe[0]);

				run(job, pindex, ackpipe[1]);

				exit(0);
			} else if (child < 0) {
				die("Can not fork()\n");
			}

			continue;
		}

		/* States 0 and 4 */
		if (!somethingtoissue && !somethingtowait)
			break;

		/* States 1, 2, 3, 5 */
		read_job_ack(&jobsdone, ackpipe[0], places, nplaces);
	}

	if (VERBOSE)
		fprintf(stderr, "All jobs done (%zd)\n", jobsdone);
}
