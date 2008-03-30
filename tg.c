#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tg.h"
#include "support.h"
#include "vplist.h"

struct tgline {
	char *src;
	char *dst;
	char *cmd;
	double value;
};

static int tg_add_edge(struct vplist *edgelist, struct tgline *tgline)
{
	struct tgedge *edge;
	char *src, *dst;

	edge = malloc(sizeof *edge);
	src = strdup(tgline->src);
	dst = strdup(tgline->dst);
	if (edge == NULL || src == NULL || dst == NULL)
		return -1;

	*edge = (struct tgedge) {.src = src,
				 .dst = dst,
				 .cost = tgline->value};

	if (vplist_append(edgelist, edge))
		return -1;

	return 0;
}

static int tg_add_node(struct vplist *nodelist, struct tgline *tgline)
{
	struct tgnode *node;
	char *cmd, *name;

	node = malloc(sizeof *node);
	cmd = strdup(tgline->cmd);
	name = strdup(tgline->src);
	if (node == NULL || cmd == NULL || name == NULL)
		return -1;

	*node = (struct tgnode) {.name = name,
				 .cmd = cmd,
				 .cost = tgline->value};

	if (vplist_append(nodelist, node))
		return -1;

	return 0;
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

static void handle_nodes(struct dgraph *tg, struct vplist *nodelist)
{
	struct vplist *lnode;
	struct tgnode *tgnode;

	/* XXX: Optimize O(n^2) -> O(n log n) */
	while ((tgnode = vplist_pop_head(nodelist)) != NULL) {

		VPLIST_FOR_EACH(lnode, nodelist) {
			if (strcmp(lnode->item, tgnode->name) == 0)
				die("Duplicate node %s\n", tgnode->name);
		}

		if (agl_add_node(tg, tgnode))
			die("Can not add node %s\n", tgnode->name);
	}
}

static int parse_line(struct vplist *nodelist, struct vplist *edgelist,
		      char *line)
{
	int namei, valuei, dsti, cmdi, tokeni, nexti;
	char *endptr;
	int isedge;
	struct tgline tgline = {.src = NULL};

	/* Parse commands of the form:
	 * name value cmd...
	 * name -> name value
	 */

	/* Get name index */
	namei = get_next_and_terminate(&nexti, line, 0);
	if (namei < 0)
		return -1;

	tgline.src = &line[namei];

	/* Get value index or "->" */
	tokeni = get_next_and_terminate(&nexti, line, nexti);
	if (tokeni < 0)
		return -1;

	isedge = (strcmp(&line[tokeni], "->") == 0);

	if (isedge) {
		/* Got an edge line */
		dsti = get_next_and_terminate(&nexti, line, nexti);
		if (dsti < 0)
			return -1;

		tgline.dst = &line[dsti];

		valuei = skipws(line, nexti);
		if (valuei < 0)
			return -1;
	} else {
		/* Got a node line: token index is a value index */
		valuei = tokeni;

		/* The rest of the line from offset cmdi is the actual
		   job command that is executed */
		cmdi = skipws(line, nexti);
		if (cmdi < 0)
			return -1;

		tgline.cmd = &line[cmdi];
	}

	tgline.value = strtod(&line[valuei], &endptr);

	if (*endptr != 0 || tgline.value < 0) {
		fprintf(stderr, "Invalid value: %s\n", &line[valuei]);
		return -1;
	}

	if (isedge) {
		if (tg_add_edge(edgelist, &tgline))
			return -1;
	} else {
		if (tg_add_node(nodelist, &tgline))
			return -1;
	}

	return 0;
}

void tg_parse_jobfile(struct jobqueue *queue, char *jobfilename)
{
	struct tgjobs *tgjobs = (struct tgjobs *) queue->data;
	char line[MAX_CMD_SIZE];
	FILE *jobfile;
	ssize_t ret;
	size_t lineno = 0;
	struct vplist nodelist = VPLIST_INITIALIZER;
	struct vplist edgelist = VPLIST_INITIALIZER;

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
		lineno++;

		ret = read_stripped_line(line, sizeof line, jobfile);
		if (ret < 0)
			break;

		if (!useful_line(line))
			continue;

		if (parse_line(&nodelist, &edgelist, line))
			die("Invalid TG line: %s:%zd\n", jobfilename, lineno);
	}

	handle_nodes(tgjobs->tg, &nodelist);

	fclose(jobfile);
}
