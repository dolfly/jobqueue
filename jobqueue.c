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


#define MAX_CMD_SIZE 65536


#define die(fmt, args...) do { \
    fprintf(stderr, "%s:%d: %s: " fmt, __FILE__, __LINE__, __func__, ## args); \
    exit(1); \
  } while(0)


static const char *USAGE =
"\n"
"SYNTAX:\n"
"\tjobqueue [-n x] [FILE ...]\n"
"\n"
"jobqueue is a tool for executing lists of jobs on several processors or\n"
"machines in parallel. jobqueue reads commands (jobs) from files, or if no\n"
"files are given, jobqueue reads commands from stdin. Each command is a line\n"
"read from FILE or stdin. Each job is executed\n"
"by giving an additional place id as a parameter for the command.\n"
"Place id defines a virtual execution place for the job. Place id\n"
"can be used for multiprocessing jobs on several processors or machines.\n"
"The place id is an integer from 0 to (x - 1).  By default x is 1, but\n"
"\"-n x\" can be used to set it. Jobqueue keeps at most x jobs running\n"
"and issues new jobs as the running jobs end.\n"
"\n"
"EXAMPLE: run echo 5 times printing the execution\n"
"\n"
"\tfor i in $(seq 5) ; do echo echo ; done |jobqueue -n2\n"
"\n"
"prints something like \"0 1 0 1 0\"\n";


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


static void run(char *cmd, int ps, int fd)
{
	ssize_t ret;
	char run_cmd[MAX_CMD_SIZE];

	ret = snprintf(run_cmd, sizeof run_cmd, "%s %d", cmd, ps);
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
		/* fgets() may be unable  to restart after SIGCHLD:
		   it can return NULL, and therefore feof() and ferror() need
		   be tested */
		if (feof(f) || ferror(f))
			return -1;
	}

	len = strlen(buf);

	if (buf[len - 1] == '\n') {
		len--;
		buf[len] = 0;
	}

	return len;
}


static void schedule(int nprocesses, FILE *joblist)
{
	char cmd[MAX_CMD_SIZE];
	char *busy;
	int pindex;
	int p[2];
	ssize_t ret;
	size_t jobsread = 0;
	size_t jobsdone = 0;
	int exitmode = 0;

	assert(nprocesses > 0);

	if (pipe(p))
		die("Can not create a pipe\n");

	/* All processing stations are not busy in the beginning -> calloc() */
	busy = calloc(nprocesses, 1);
	if (busy == NULL)
		die("Out of memory\n");

	while (1) {
		/* Find a free execution place */
		for (pindex = 0; pindex < nprocesses; pindex++) {
			if (!busy[pindex])
				break;
		}

		if (exitmode || pindex == nprocesses) {
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

			busy[pindex] = 0;

			jobsdone++;

			if (exitmode && jobsread == jobsdone) {
				fprintf(stderr, "All jobs done (%zd)\n", jobsdone);
				break;
			}

		} else {

			/* Read a new job and strip \n away from the command */
			ret = read_stripped_line(cmd, sizeof cmd, joblist);
			if (ret < 0) {
				exitmode = 1;
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
	}
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

	if (sigaction(SIGCHLD, &act, NULL) < 0) {
		fprintf(stderr, "Can not install child handler: %s\n",
			strerror(errno));
		exit(1);
	}
}


int main(int argc, char *argv[])
{
	int ret;
	int n = 1;
	int l;
	char *endptr;
	FILE *joblist;
	char *jobfilename;
	int i;
	int use_stdin;

	setup_child_handler();

	while ((ret = getopt(argc, argv, "hn:v")) != -1) {
		switch (ret) {
		case 'h':
			print_help();
			exit(0);

		case 'n':
			l = strtol(optarg, &endptr, 10);
			if ((l <= 0 || l >= INT_MAX) || *endptr != 0) {
				fprintf(stderr, "Invalid parameter: %s\n", optarg);
				exit(1);
			}
			n = l;
			break;

		case 'v':
			printf("jobqueue %s\n", VERSION);
			exit(0);

		default:
			fprintf(stderr, "Impossible option.\n");
			exit(1);
		}
	}

	
	use_stdin = (optind == argc);
	i = optind;

	while (1) {
		if (use_stdin) {
			joblist = stdin;
		} else {
			if (i == argc)
				break;

			jobfilename = argv[i];
			i++;

			joblist = fopen(jobfilename, "r");

			if (joblist == NULL) {
				fprintf(stderr, "Can not open file: %s\n",
					jobfilename);
				continue;
			}
		}

		schedule(n, joblist);

		fclose(joblist);

		if (use_stdin)
			break;
	}

	return 0;
}
