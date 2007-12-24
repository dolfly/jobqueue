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
#include "jobqueue.h"
#include "schedule.h"


/* Pass an execution place id parameter for each job if executionplace != 0 */
int executionplace;
int verbosemode;

static struct vplist jobfilenames;

static const char *USAGE =
"\n"
"SYNTAX:\n"
"\tjobqueue [-i] [-n x] [-v] [--version] [FILE ...]\n"
"\n"
"jobqueue is a tool for executing lists of jobs on several processors or\n"
"machines in parallel. jobqueue reads jobs (shell commands) from files. If no\n"
"files are given, jobqueue reads jobs from stdin. Each job is executed in a\n"
"shell environment (man 3 system).\n"
"\n"
" \"-n x\", jobqueue keeps at most x jobs running in parallel.\n"
"    Jobqueue issues new jobs as older jobs are finished.\n"
"\n"
" -i, each job is executed by passing an execution place id as a parameter.\n"
"    The execution place defines a virtual execution place for the job, which\n"
"    can be used to determine a machine to execute the job.\n"
"    The place id is an integer from 1 to x (given with -n).\n"
"    If command \"foo\" is executed from a job list, jobqueue executes \"foo x\",\n"
"    where x is the execution place id.\n"
"\n"
" -v, enter verbose mode. Print each command that is executed.\n"
"\n"
" --version, print version number\n"
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
FILE *get_next_jobfile(void)
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

	enum jobqueueoptions {
		OPT_HELP    = 'h',
		OPT_VERSION = 1000,
	};

	const struct option longopts[] = {
		{.name = "help",    .has_arg = 0, .flag = NULL, .val = OPT_HELP},
		{.name = "version", .has_arg = 0, .flag = NULL, .val = OPT_VERSION},
		{.name = NULL}};

	setup_child_handler();

	while (1) {
		ret = getopt_long(argc, argv, "hin:v", longopts, NULL);
		if (ret == -1)
			break;

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
			verbosemode = 1;
			break;

		case OPT_VERSION:
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
