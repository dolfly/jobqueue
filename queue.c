#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "support.h"
#include "queue.h"
#include "vplist.h"
#include "agl/directedgraph.h"

struct tgjobs {
	size_t ngraphs;
	size_t njobs;

	struct dgraph *tg;
};

static struct vplist jobfilenames = VPLIST_INITIALIZER;


static void can_not_open_file(const char *fname)
{
	fprintf(stderr, "Can't open file %s: %s\n", fname, strerror(errno));
}

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
			can_not_open_file(fname);

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


static void tg_add_edge(struct tgjobs *tgjobs, char *src, char *value, char *dst)
{
	printf("edge %s %s %s\n", src, value, dst);
}

static void tg_add_node(struct tgjobs *tgjobs, char *name, char *value, char *cmd)
{
	printf("node %s %s %s\n", name, value, cmd);	
}

/* Read one whitespace separated entry starting from offset i, skip
 * initial whitespace if necessary. Returns -1 on failure.
 * Returns a non-negative starting offset on success. On the next call
 * scanning can be continued from the offset filled into *nexti.
 */
static int get_next_and_terminate(int *nexti, char *line, int i)
{
	i = skipws(line, i);
	if (i < 0)
		return -1;

	*nexti = skipnws(line, i);
	if (*nexti < 0)
		return -1;

	line[*nexti] = 0;

	(*nexti)++;

	return i;
}

static void tg_add_jobfile(struct jobqueue *queue, char *jobfilename)
{
	struct tgjobs *tgjobs = (struct tgjobs *) queue->data;
	char line[MAX_CMD_SIZE];
	FILE *jobfile;
	int namei, dstnamei, valuei, tokeni, cmdi, nexti;
	ssize_t ret;

	if (tgjobs == NULL) {
		tgjobs = calloc(1, sizeof(struct tgjobs));
		if (tgjobs == NULL)
			die("No memory for tgjobs structure\n");

		tgjobs->tg = agl_create(0, NULL);
		if (tgjobs->tg == NULL)
			die("Can not create a tg\n");

		queue->data = tgjobs;
	}

	jobfile = fopen(jobfilename, "r");
	if (jobfile == NULL) {
		can_not_open_file(jobfilename);
		return;
	}

	while (1) {
		ret = read_stripped_line(line, sizeof line, jobfile);
		if (ret < 0)
			break;

		if (!useful_line(line))
			continue;

		/* Parse commands of the form:
		 * name value cmd...
		 * name -> name value
		 */

		/* Get name index */
		namei = get_next_and_terminate(&nexti, line, 0);
		if (namei < 0)
			continue;

		/* Get value index or "->" */
		tokeni = get_next_and_terminate(&nexti, line, nexti);
		if (tokeni < 0)
			continue;

		if (strcmp(&line[tokeni], "->") == 0) {
			/* Got an edge line */
			dstnamei = get_next_and_terminate(&nexti, line, nexti);
			if (dstnamei < 0)
				continue;

			valuei = skipws(line, nexti);
			if (valuei < 0)
				continue;

			tg_add_edge(tgjobs, &line[namei], &line[valuei], &line[dstnamei]);
		} else {
			/* Got a job line: token index is a value index */
			valuei = tokeni;

			/* The rest of the line from offset cmdi is the actual
			   job command that is executed */
			cmdi = skipws(line, nexti);
			if (cmdi < 0)
				continue;

			tg_add_node(tgjobs, &line[namei], &line[valuei], &line[cmdi]);
		}
	}

	fclose(jobfile);
}

static int tg_next(char *cmd, size_t maxlen, struct jobqueue *queue)
{
	return 0;
}

struct jobqueue *init_queue(char *argv[], int i, int argc, int taskgraphmode)
{
	char *jobfilename;
	int use_stdin;
	struct jobqueue *queue;

	queue = calloc(1, sizeof(struct jobqueue));
	if (queue == NULL)
		die("Not enough memory for struct jobqueue\n");

	queue->next = taskgraphmode ? tg_next : cq_next;

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

		if (taskgraphmode) {
			tg_add_jobfile(queue, jobfilename);
			free(jobfilename);
			jobfilename = NULL;
		} else {
			if (vplist_append(&jobfilenames, jobfilename))
				die("No memory for jobs\n");
		}

		if (use_stdin)
			break;
	}

	return queue;
}
