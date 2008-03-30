#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tg.h"
#include "support.h"
#include "vplist.h"

static int tg_add_edge(struct vplist *edgelist,
		       char *src, char *value, char *dst, size_t lineno)
{
	double cost;
	char *endptr;
	struct tgedge *edge;

	cost = strtod(value, &endptr);
	if (*endptr != 0)
		return -1;

	edge = malloc(sizeof *edge);
	src = strdup(src);
	dst = strdup(dst);
	if (edge == NULL || src == NULL || dst == NULL)
		return -1;

	*edge = (struct tgedge) {.lineno = lineno,
				 .src = src,
				 .dst = dst,
				 .cost = cost};

	if (vplist_append(edgelist, edge))
		return -1;

	return 0;
}

static int tg_add_node(struct vplist *nodelist,
		       char *name, char *value, char *cmd)
{
	double cost;
	char *endptr;
	struct tgnode *node;

	cost = strtod(value, &endptr);
	if (*endptr != 0)
		return -1;

	node = malloc(sizeof *node);
	cmd = strdup(cmd);
	name = strdup(name);
	if (node == NULL || cmd == NULL || node == NULL)
		return -1;

	*node = (struct tgnode) {.name = name,
				 .cmd = cmd,
				 .cost = cost};

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

	while ((tgnode = vplist_pop_head(nodelist)) != NULL) {

		VPLIST_FOR_EACH(lnode, nodelist) {
			if (strcmp(lnode->item, tgnode->name) == 0)
				die("Duplicate node %s\n", tgnode->name);
		}

		if (agl_add_node(tg, tgnode))
			die("Can not add node %s\n", tgnode->name);
	}
}

void tg_parse_jobfile(struct jobqueue *queue, char *jobfilename)
{
	struct tgjobs *tgjobs = (struct tgjobs *) queue->data;
	char line[MAX_CMD_SIZE];
	FILE *jobfile;
	int namei, dstnamei, valuei, tokeni, cmdi, nexti;
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

		/* Parse commands of the form:
		 * name value cmd...
		 * name -> name value
		 */

		/* Get name index */
		namei = get_next_and_terminate(&nexti, line, 0);
		if (namei < 0)
			goto invalidline;

		/* Get value index or "->" */
		tokeni = get_next_and_terminate(&nexti, line, nexti);
		if (tokeni < 0)
			goto invalidline;

		if (strcmp(&line[tokeni], "->") == 0) {
			/* Got an edge line */
			dstnamei = get_next_and_terminate(&nexti, line, nexti);
			if (dstnamei < 0)
				goto invalidline;

			valuei = skipws(line, nexti);
			if (valuei < 0)
				goto invalidline;

			if (tg_add_edge(&edgelist, &line[namei], &line[valuei], &line[dstnamei], lineno))
				dieerror("Can not add an edge: %s:%zd\n",
					 jobfilename, lineno);
					 
		} else {
			/* Got a job line: token index is a value index */
			valuei = tokeni;

			/* The rest of the line from offset cmdi is the actual
			   job command that is executed */
			cmdi = skipws(line, nexti);
			if (cmdi < 0)
				goto invalidline;

			tg_add_node(&nodelist, &line[namei], &line[valuei], &line[cmdi]);
		}
	}

	handle_nodes(tgjobs->tg, &nodelist);

	fclose(jobfile);

	return;

 invalidline:
	die("Invalid line %zd in file %s\n", lineno, jobfilename);
}
