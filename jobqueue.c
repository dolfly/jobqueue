#define _GNU_SOURCE

#include <getopt.h>
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
#include <signal.h>


#include "version.h"
#include "vplist.h"


#define MAX_CMD_SIZE 65536


#define die(fmt, args...) do { \
    fprintf(stderr, fmt, ## args); \
    exit(1); \
  } while(0)


/* Pass an execution place id parameter for each job if executionplace != 0 */
static int executionplace = 0;

static struct vplist jobfilenames;

static const char *USAGE =
"\n"
"SYNTAX:\n"
"\tjobqueue [-i] [-n x] [FILE ...]\n"
"\n"
"jobqueue is a tool for executing lists of jobs on several processors or\n"
"machines in parallel. jobqueue reads jobs (shell commands) from files. If no\n"
"files are given, jobqueue reads jobs from stdin. Each job is executed in a\n"
"shell environment (man 3 system).\n"
"\n"
"If \"-n x\" is used, jobqueue keeps at most x jobs running in parallel.\n"
"Jobqueue issues new jobs as older jobs are finished.\n"
"\n"
"If -i is given, each job is executed by passing it an execution place id.\n"
"The execution place defines a virtual execution place for the job, which\n"
"can be used to determine a machine to execute the job.\n"
"The place id is an integer from 1 to x (given with -n).\n"
"\n"
"EXAMPLE 1: A file named MACHINES contains a list of machines to process\n"
"jobs from a job file named JOBS. Each line in the JOBS file follows the\n"
"same pattern:\n"
"./myscript data0\n"
"./myscript data1\n"
"./myscript data2\n"
"...\n"
"\n"
"MACHINES file contains 4 machines:\n"
"machine0\n"
"machine1\n"
"machine2\n"
"machine3\n"
"\n"
"./myscript might do something like this:\n"
"#!/bin/bash\n"
"\n"
"# This is the dataX parameter from JOBS file\n"
"data=\"$1\"\n"
"# This is the execution place passed from the jobqueue: in range 0-3\n"
"machinenumber=$2\n"
"\n"
"# determine the machine that will execute this job\n"
"machine=$(head -n $machinenumber MACHINES |tail -n1)\n"
"\n"
"echo $machine $data\n"
"ssh $machine remotecommand $data\n"
"\n"
"To execute jobs on the machines, issue:\n"
"\n"
"jobqueue -n 4 -i JOBS\n"
"\n"
"Execution will print something like this:\n"
"machine0 data0\n"
"machine1 data1\n"
"machine2 data2\n"
"machine1 data4\n"
"machine3 data3\n"
"machine0 data5\n"
"machine1 data7\n"
"machine2 data6\n"
"All jobs done (8)\n"
"\n"
"EXAMPLE 2: run echo 5 times printing the execution place each time\n"
"\n"
"\tfor i in $(seq 5) ; do echo echo ; done |jobqueue -n2 -i\n"
"\n"
"prints something like \"1 2 1 2 1\".\n";


/* We try to fopen() each file in the jobfiles list, and return the first
 * successfully opened file. Otherwise return NULL.
 */
static FILE *get_next_jobfile(void)
{
	char *fname;
	FILE *f = NULL;

	do {
		fname = vplist_pop_head(&jobfilenames);

		if (fname == NULL)
			break;

		f = fopen(fname, "r");

		if (f == NULL)
			fprintf(stderr, "jobqueue: can't open %s\n", fname);

		free(fname);

	} while (f == NULL);

	return f;
}


static void print_help(void)
{
	printf("jobqueue %s by Heikki Orsila <heikki.orsila@iki.fi>\n", VERSION);
	printf("%s", USAGE);
}



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

	ret = system(run_cmd);
	if (ret == -1)
		die("job delivery failed: %s\n", run_cmd);

	/* pipe(7) guarantees write atomicity when
	   sizeof(ps) <= PIPE_BUF */
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


static void schedule(int nprocesses)
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

	if (pipe(p))
		die("Can not create a pipe\n");

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


static void trivial_sigchld(int signum)
{
	while (waitpid(-1, NULL, WNOHANG) > 0);
}


static void setup_child_handler(void)
{
	struct sigaction act = (struct sigaction) {
		.sa_handler = trivial_sigchld,
		.sa_flags = SA_NOCLDSTOP};

	if (sigaction(SIGCHLD, &act, NULL) < 0)
		die("Can not install child handler: %s\n", strerror(errno));
}


int main(int argc, char *argv[])
{
	int ret;
	int n = 1;
	int l;
	char *endptr;
	char *jobfilename;
	int i;
	int use_stdin;

	setup_child_handler();

	while ((ret = getopt(argc, argv, "hin:v")) != -1) {
		switch (ret) {
		case 'h':
			print_help();
			exit(0);

		case 'i':
			executionplace = 1;
			break;

		case 'n':
			l = strtol(optarg, &endptr, 10);

			if ((l <= 0 || l >= INT_MAX) || *endptr != 0)
				die("Invalid parameter: %s\n", optarg);

			n = l;
			break;

		case 'v':
			printf("jobqueue %s\n", VERSION);
			exit(0);

		default:
			die("Impossible option\n");
		}
	}

	vplist_init(&jobfilenames);
	
	use_stdin = (optind == argc);
	i = optind;

	while (1) {
		if (use_stdin) {
			jobfilename = strdup("/dev/stdin");

		} else {
			if (i == argc)
				break;

			jobfilename = strdup(argv[i]);
			i++;
		}

		if (jobfilename == NULL)
			die("Can not allocate a filename for a job list\n");

		if (vplist_append(&jobfilenames, jobfilename))
			die("No memory for jobs\n");

		if (use_stdin)
			break;
	}

	schedule(n);

	return 0;
}
