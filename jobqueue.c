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

#include "version.h"


#define MAX_CMD_SIZE 65536


#define die(fmt, args...) do { \
    fprintf(stdout, "%s:%d: %s: " fmt, __FILE__, __LINE__, __func__, ## args); \
    exit(1); \
  } while(0)


void print_help(void)
{
	printf("Help not implemented\n");
}


void schedule(int n, FILE *joblist)
{
	char cmd[MAX_CMD_SIZE];
	char *stations;
	int i;

	assert(n > 0);

	stations = calloc(n, 1);
	if (stations == NULL)
		die("Out of memory\n");

	while (fgets(cmd, sizeof cmd, joblist) != NULL) {

		for (i = 0; i < n; i++) {
			if (stations[i] != 0)
				break;
		}

		
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
