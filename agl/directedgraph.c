#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "directedgraph.h"

/* Append item to the end of array that has n elements used,
 * nallocated elements already allocated. success is set to 0 on success,
 * -1 otherwise
 */
#define darray_append(success, n, nallocated, array, item) do { \
	assert((n) >= 0); \
	assert((nallocated) >= 0); \
	assert((n) <= (nallocated)); \
	(success) = 0; \
	if ((n) == (nallocated)) { \
		void *_n; \
		if ((nallocated) == 0) \
			(nallocated) = 1; \
		(nallocated) *= 2; \
		assert((nallocated) > 0); \
		_n = realloc((array), (nallocated) * sizeof((array)[0])); \
		if (_n == NULL) \
			(success) = -1; \
		else \
			(array) = _n; \
	} \
	if (success == 0) { \
		(array)[(n)] = (item); \
		(n)++; \
	} \
} while (0)


static inline size_t sat_mul_sizet(size_t a, size_t b)
{
	size_t limit = ((size_t) (-1)) / a;

	if (b > limit)
		return -1;

	return a * b;
}

static void deinit_node(struct dgnode *node)
{
	free(node->in);
	free(node->out);

	memset(node, 0, sizeof *node);
}

int agl_add_edge(struct dgraph *graph, size_t src, size_t dst, void *data)
{
	struct dgnode *sn, *dn;
	struct dgedge tmpedge = {.src = src,
				 .dst = dst,
				 .data = data};
	struct dgedge *newedge;
	int success;

	assert(src < graph->n);
	assert(dst < graph->n);

	sn = &graph->nodes[src];
	dn = &graph->nodes[dst];

	darray_append(success, sn->nout, sn->nallocatedout, sn->out, tmpedge);
	if (success)
		return -1;

	/* The new edge is at the end of the edge array */
	newedge = &sn->out[sn->nout - 1];

	darray_append(success, dn->nin, dn->nallocatedin, dn->in, newedge);
	if (success) {
		/* We don't need to free anything even if there are two
		   darray_append() calls */
		sn->nout--;
		return -1;
	}

	return 0;
}

int agl_add_node(struct dgraph *graph, void *data)
{
	int success;
	struct dgnode node = {.i = graph->n,
			      .data = data};

	darray_append(success, graph->n, graph->allocated, graph->nodes, node);

	return success;
}

double *agl_b_levels(struct dgraph *graph,
		     double (*nf)(struct dgnode *node, void *data),
		     double (*ef)(struct dgedge *node, void *data),
		     void *data)
{
	size_t *tsortorder = NULL;
	int cyclic;
	double *priorities = NULL;
	struct dgnode *node, *child;
	struct dgedge *edge;
	double nodecost, maximum, pri;
	size_t nodei, i;

	if (graph->n == 0)
		goto error;

	tsortorder = agl_topological_sort(&cyclic, graph);
	if (tsortorder == NULL)
		goto error;

	priorities = calloc(graph->n, sizeof priorities[0]);
	if (priorities == NULL)
		goto error;

	for (i = graph->n; i > 0;) {
		i--;
		nodei = tsortorder[i];
		node = &graph->nodes[nodei];

		nodecost = (nf != NULL) ? nf(node, data) : 1.0;
		maximum = nodecost;

		AGL_FOR_EACH_EDGE(node, edge) {
			child = &graph->nodes[edge->dst];

			pri = priorities[child->i] + nodecost;

			if (ef)
				pri += ef(edge, data);

			if (pri > maximum)
				maximum = pri;
		}
		AGL_END_FOR_EACH_EDGE();

		priorities[nodei] = maximum;
	}

 error:
	free(tsortorder);
	return priorities;
}

struct dgraph *agl_create(size_t nnodeshint, void *data)
{
	struct dgraph *dg = malloc(sizeof(struct dgraph));

	if (dg == NULL)
		return NULL;

	if (agl_init(dg, nnodeshint, data)) {
		free(dg);
		return NULL;
	}

	return dg;
}

void agl_deinit(struct dgraph *graph)
{
	size_t i;

	for (i = 0; i < graph->n; i++)
		deinit_node(&graph->nodes[i]);

	free(graph->nodes);

	memset(graph, 0, sizeof *graph);
}

int agl_dfs(struct dgraph *graph, size_t initial, char *visited, size_t *fin,
	    int (*f)(struct dgnode *node, void *data), void *data)
{
	size_t src, dst, j;
	size_t n = 0;
	size_t allocated = 0;
	size_t *stack = NULL;
	struct dgnode *node;
	int ret = 0;
	int visitedallocated = 0;
	int success;
	int added;
	size_t time = 0;

	if (graph->n == 0)
		return 0;

	assert(initial < graph->n);

	if (visited == NULL) {
		visited = calloc(graph->n, 1);
		if (visited == NULL) {
			ret = -1;
			goto out;
		}

		visitedallocated = 1;
	}

	darray_append(success, n, allocated, stack, initial);
	if (success) {
		ret = -1;
		goto out;
	}

	while (n > 0) {
		/* Pop the last value from the stack */
		src = stack[n - 1];
		assert(src < graph->n);

		node = &graph->nodes[src];

		if (!visited[src]) {
			/* Mark node as grey (at least once visited):
			   value == 1 */
			visited[src] = 1;

			if (f != NULL && f(node, data)) {
				ret = 1;
				break;
			}
		}

		added = 0;

		for (j = 0; j < node->nout; j++) {
			dst = node->out[j].dst;
			assert(dst < graph->n);

			if (visited[dst]) {
				/* If the node is grey, mark it is a node
				   that belongs to a cycle: value == 3 */
				if (visited[dst] == 1)
					visited[dst] = 3;

				continue;
			}

			darray_append(success, n, allocated, stack, dst);
			if (success) {
				ret = -1;
				goto out;
			}

			added++;
		}

		if (!added) {
			n--;

			/* Mark node as black, if it doesn't belong to
			   a cycle: value == 2 */
			if (visited[src] != 3)
				visited[src] = 2;

			if (fin)
				fin[src] = time;

			time++;
		}
	}
 out:
	if (visitedallocated)
		free(visited);

	free(stack);

	return ret;
}

void agl_free(struct dgraph *graph)
{
	agl_deinit(graph);
	free(graph);
}

int agl_has_cycles(struct dgraph *graph)
{
	char *visited = NULL;
	size_t i;
	int ret = 0;

	if (graph->n == 0)
		goto out;

	visited = calloc(graph->n, 1);
	if (visited == NULL) {
		ret = -1;
		goto out;
	}

	if (agl_dfs(graph, 0, visited, NULL, NULL, NULL)) {
		ret = -1;
		goto out;
	}

	for (i = 0; i < graph->n; i++) {
		if (visited[i] == 3) {
			ret = 1;
			break;
		}
	}

 out:
	free(visited);

	return ret;
}

int agl_init(struct dgraph *graph, size_t nnodeshint, void *data)
{
	size_t s;

	*graph = (struct dgraph) {.n = 0};

	graph->allocated = (nnodeshint > 0) ? nnodeshint : 4;

	s = sat_mul_sizet(sizeof(struct dgnode), graph->allocated);
	if (s == -1)
		return -1;

	graph->nodes = calloc(s, 1);
	if (graph->nodes == NULL)
		return -1;

	graph->data = data;

	return 0;
}

/* toposortcmp(a, b) returns -1 if b < a, 0 if a == b, and 1 if a < b.
 * That is, the array is sorted in decreasing value order
 */
static int toposortcmp(const void *a, const void *b)
{
	const size_t *ra =  a;
	const size_t *rb =  b;

	if (*ra < *rb)
		return 1;
	else if (*rb < *ra)
		return -1;

	return 0;
}

size_t *agl_topological_sort(int *cyclic, struct dgraph *graph)
{
	size_t *fin = NULL;
	char *visited;
	size_t *order = NULL;
	size_t src;

	assert(cyclic != NULL);

	*cyclic = 0;

	if (graph->n == 0)
		goto error;

	order = malloc(sizeof(order[0]) * graph->n);
	if (order == NULL)
		goto error;

	/* A trick, malloc twice the size to be able to sort later */
	fin = malloc(2 * sizeof(fin[0]) * graph->n);
	if (fin == NULL)
		goto error;

	/* A trick, we can use order array as DFS visited array because
	   we need the order array much later */
	visited = (char *) order;

	memset(visited, 0, graph->n);

	/* Do DFS for all nodes */
	for (src = 0; src < graph->n; src++) {
		if (visited[src])
			continue;

		if (agl_dfs(graph, src, visited, fin, NULL, NULL))
			goto error;
	}

	/* Cycle test */
	for (src = 0; src < graph->n; src++) {
		if (visited[src] == 3) {
			*cyclic = 1;
			goto error;
		}
	}

	/* In the fin array, there are finish times for each node:
	   (fin_0, fin_1, ..., fin_n-1). We expand this array to
	   (fin_0, 0, fin_1, 1, ..., fin_n-1, n-1). We allocated twice the
	   size array to do this trick */
	for (src = graph->n; src > 0;) {
		src--;
		fin[2 * src] = fin[src];
		fin[2 * src + 1] = src;
	}

	/* Then we sort the list to descending finish time order */
	qsort(fin, graph->n, 2 * sizeof(fin[0]), toposortcmp);

	/* And finally, we return the desired list containing node numbers
	   in descending finish time order */
	for (src = 0; src < graph->n; src++)
		order[src] = fin[src * 2 + 1];

	goto out;

 error:
	free(order);
	order = NULL;
 out:
	free(fin);

	return order;
}
