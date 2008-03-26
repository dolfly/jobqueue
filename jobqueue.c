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
#include "support.h"

struct vplist machinelist = VPLIST_INITIALIZER;

int maxissue = 1;

/* Pass an execution place id parameter for each job if
 * passexecutionplace != 0 */
int passexecutionplace;

int requeuefailedjobs;

int verbosemode;

static const char *USAGE =
"\n"
"SYNTAX:\n"
"\tjobqueue [-e] [-n x] [-m list] [--max-restart=x] [-r] [-v] [--version]\n"
"\t         [-x n] [FILE ...]\n"
"\n"
"jobqueue is a tool for executing lists of jobs on several processors or\n"
"machines in parallel. jobqueue reads jobs (shell commands) from files. If no\n"
"files are given, jobqueue reads jobs from stdin. Each job is executed in a\n"
"shell environment (man 3 system).\n"
"\n"
" -n x / --nodes=x, jobqueue keeps at most x jobs running in parallel.\n"
"    Jobqueue issues new jobs as older jobs are finished.\n"
"\n"
" -e / --execution-place, each job is executed by passing an execution place id\n"
"    as a parameter. The execution place defines a virtual execution place for\n"
"    the job, which can be used to determine a machine to execute the job.\n"
"    The place id is an integer from 1 to x (given with -n).\n"
"    If command \"foo\" is executed from a job list, jobqueue executes \"foo x\",\n"
"    where x is the execution place id.\n"
"\n"
" -m list / --machine-list list, read contents of list file, and count each\n"
"    non-empty and non-comment line to be an execution place. Pass execution\n"
"    place for each executed job as a parameter. The difference to -e is that\n"
"    -e passes the execution place as an integer, but this option passes the\n"
"    execution place as a name for the job. Also, this option implies \"-n x\",\n"
"    where x is the number of names read from the file.\n"
"\n"
" --max-restart=x, implies -r / --restart-failed, but sets the maximum number\n"
"    of restarts for each job\n"
"\n"
" -r / --restart-failed, if a job that is executed returns an error code, it is\n"
"    restarted (on some execution place). If the error code is 1, the\n"
"    job simply failed and it is restarted. If the error code is 2, the\n"
"    execution place is marked as being failed, and thus, no additional jobs\n"
"    will be started on that node. WARNING: There is no limit for maximum\n"
"    number of restarts unless --max-restart is used.\n"
"\n"
" -v / --verbose, enter verbose mode. Print each command that is executed.\n"
"\n"
" --version, print version number\n"
"\n"
" -x n / --max-issue=n, set number of simultaneously running jobs for each\n"
"    execution place. For example: -m machinelist -x 2 keeps two simultaneous\n"
"    jobs running on each machine.\n"
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
"\n"
"# determine the machine that will execute this job\n"
"machine=$2\n"
"\n"
"echo $machine $data\n"
"ssh $machine remotecommand $data\n"
"\n"
"To execute jobs on the machines, issue:\n"
"\n"
"jobqueue -m MACHINES JOBS\n"
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
"\tfor i in $(seq 5) ; do echo echo ; done |jobqueue -n2 -e\n"
"\n"
"prints something like \"1 2 1 2 1\".\n";


static void print_help(void)
{
	printf("jobqueue %s by Heikki Orsila <heikki.orsila@iki.fi>\n", VERSION);
	printf("%s", USAGE);
}


static int read_machine_list(const char *fname)
{
	FILE *f;
	char name[256];
	size_t nmachines = 0;
	char *nameptr;
	ssize_t len;

	if (machinelist.next != NULL)
		die("You may not specify machine list twice\n");

	f = fopen(fname, "r");

	while (1) {
		len = read_stripped_line(name, sizeof name, f);
		if (len < 0)
			break;

		if (!useful_line(name))
			continue;

		nameptr = strdup(name);
		if (nameptr == NULL)
			die("Not enough memory for machine list\n");

		if (vplist_append(&machinelist, nameptr))
			die("Not enough memory for machine list\n");

		nmachines++;
	}

	fclose(f);

	return nmachines;
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
	int nplaces = 1;
	int nplacespassed = 0;
	int l;
	char *endptr;
	struct jobqueue *queue;
	int taskgraphmode = 0;

	enum jobqueueoptions {
		OPT_EXECUTION_PLACE = 'e',
		OPT_HELP            = 'h',
		OPT_MACHINE_LIST    = 'm',
		OPT_MAX_RESTART     = 1000,
		OPT_NODES           = 'n',
		OPT_RESTART_FAILED  = 'r',
		OPT_MAX_ISSUE       = 'x',
		OPT_TASK_GRAPH      = 't',
		OPT_VERBOSE         = 'v',
		OPT_VERSION         = 1001,
	};

	const struct option longopts[] = {
		{.name = "help",            .has_arg = 0, .val = OPT_HELP},
		{.name = "execution-place", .has_arg = 0, .val = OPT_EXECUTION_PLACE},
		{.name = "machine-list",    .has_arg = 1, .val = OPT_MACHINE_LIST},
		{.name = "max-issue",       .has_arg = 1, .val = OPT_MAX_ISSUE},
		{.name = "max-restart",     .has_arg = 1, .val = OPT_MAX_RESTART},
		{.name = "nodes",           .has_arg = 1, .val = OPT_NODES},
		{.name = "restart-failed",  .has_arg = 0, .val = OPT_RESTART_FAILED},
		{.name = "task-graph",      .has_arg = 0, .val = OPT_TASK_GRAPH},
		{.name = "verbose",         .has_arg = 0, .val = OPT_VERBOSE},
		{.name = "version",         .has_arg = 0, .val = OPT_VERSION},
		{.name = NULL}};

	setup_child_handler();
	
	while (1) {
		ret = getopt_long(argc, argv, "ehm:n:rtvx:", longopts, NULL);
		if (ret == -1)
			break;

		switch (ret) {
		case OPT_EXECUTION_PLACE:
			passexecutionplace = 1;
			break;

		case OPT_HELP:
			print_help();
			exit(0);

		case OPT_MACHINE_LIST:
			nplaces = read_machine_list(optarg);
			break;

		case OPT_NODES:
			l = strtol(optarg, &endptr, 10);

			if ((l <= 0 || l >= INT_MAX) || *endptr != 0)
				die("Invalid parameter: %s\n", optarg);

			nplaces = l;

			nplacespassed = 1;
			break;

		case OPT_RESTART_FAILED:
			if (!requeuefailedjobs)
				requeuefailedjobs = INT_MAX;
			break;

		case OPT_TASK_GRAPH:
			taskgraphmode = 1;
			break;

		case OPT_VERBOSE:
			verbosemode = 1;
			break;

		case OPT_MAX_ISSUE:
			l = strtol(optarg, &endptr, 10);

			if ((l <= 0 || l >= INT_MAX) || *endptr != 0)
				die("Invalid parameter: -x %s\n", optarg);

			maxissue = l;
			break;

		case OPT_MAX_RESTART:
			l = strtol(optarg, &endptr, 10);

			if ((l < 0) || l >= INT_MAX || *endptr != 0)
				die("Invalid parameter: %s\n", optarg);

			requeuefailedjobs = l;
			break;

		case OPT_VERSION:
			printf("jobqueue %s\n", VERSION);
			exit(0);

		default:
			die("Impossible option\n");
		}
	}

	if ((passexecutionplace && machinelist.next != NULL) ||
	    (nplacespassed && machinelist.next != NULL))
		die("Error: -m MACHINELIST may not be used with -e and -n\n");

	queue = init_queue(argv, optind, argc, taskgraphmode);

	schedule(nplaces, queue);

	return 0;
}
