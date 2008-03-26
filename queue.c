#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "support.h"
#include "queue.h"
#include "vplist.h"

static struct vplist jobfilenames = VPLIST_INITIALIZER;

/* We try to fopen() each file in the jobfiles list, and return the first
 * successfully opened file. Otherwise return NULL.
 */
static FILE *cq_get_next_jobfile(void)
{
	char *fname;
	FILE *f = NULL;

	do {
		fname = vplist_pop_head(&jobfilenames);

		if (fname == NULL)
			break;

		f = fopen(fname, "r");

		if (f)
			closeonexec(fileno(f));
		else
			fprintf(stderr, "Can't open file %s: %s\n", fname, strerror(errno));

		free(fname);

	} while (f == NULL);

	return f;
}

static int cq_next(char *cmd, size_t maxlen, struct jobqueue *queue)
{
	ssize_t ret;
	FILE *jobfile = (FILE *) queue -> data;

	while (1) {
		if (jobfile == NULL) {
			jobfile = cq_get_next_jobfile();

			if (jobfile == NULL)
				break;
		}

		/* Read a new job and strip \n away from the command */
		ret = read_stripped_line(cmd, maxlen, jobfile);
		if (ret < 0) {
			fclose(jobfile);
			jobfile = NULL;
			continue;
		}

		if (useful_line(cmd))
			break;
	}

	queue->data = jobfile;

	return jobfile != NULL;
}

struct jobqueue *cq_init(char *argv[], int i, int argc)
{
	char *jobfilename;
	int use_stdin;
	struct jobqueue *queue;

	queue = malloc(sizeof(struct jobqueue));
	if (queue == NULL)
		die("Not enough memory for struct jobqueue\n");

	*queue = (struct jobqueue) {.next = cq_next};

	use_stdin = (i == argc);

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

	return queue;
}
