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


static void print_help(void)
{
	printf("Help not implemented\n");
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


void schedule(int n, FILE *joblist)
{
	char cmd[MAX_CMD_SIZE];
	char *busy;
	int ps;
	int p[2];
	ssize_t ret;
	size_t len;
	size_t jobs = 0;

	assert(n > 0);

	if (pipe(p))
		die("Can not create a pipe\n");

	/* All processing stations are not busy in the beginning -> calloc() */
	busy = calloc(n, 1);
	if (busy == NULL)
		die("Out of memory\n");

	while (1) {
		for (ps = 0; ps < n; ps++) {
			if (!busy[ps])
				break;
		}

		if (ps == n) {
			ret = read(p[0], &ps, sizeof ps);

			if (ret == 0)
				die("Job queue was broken\n");

			assert(ret == sizeof(ps));

			busy[ps] = 0;

		} else {
			/* Read a new job and strip \n away from the command */
			if (fgets(cmd, sizeof cmd, joblist) == NULL) {
				if (!feof(joblist))
					continue;

				fprintf(stderr, "All jobs done (%zd)\n", jobs);
				break;
			}

			len = strlen(cmd);

			if (cmd[len - 1] == '\n') {
				len--;
				cmd[len] = 0;
			}

			/* Ignore empty lines and lines beginning with '#' */
			if (len == 0 || cmd[0] == '#')
				continue;

			jobs++;
			busy[ps] = 1;

			if (fork() == 0) {
				/* Child closes some file descriptors */
				close(0);
				close(p[0]);

				run(cmd, ps, p[1]);

				exit(0);
			}
		}
	}
}


static void trivial_sigchld(int signum)
{
	int status;
	waitpid(-1, &status, WNOHANG);
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
	int i;
	int use_stdin;

	setup_child_handler();

	while ((ret = getopt(argc, argv, "n:h")) != -1) {
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
			joblist = fopen(argv[i], "r");
			i++;
		}

		schedule(n, joblist);

		fclose(joblist);

		if (use_stdin)
			break;
	}

	return 0;
}
