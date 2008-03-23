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

int agl_dfs(struct dgraph *graph, size_t initial, char *visited,
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

	assert(initial < graph->n);

	if (visited == NULL) {
		visited = calloc(1, graph->n);
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
		n--;
		src = stack[n];
		assert(src < graph->n);

		/* Mark node as visited */
		visited[src] = 1;

		node = &graph->nodes[src];

		if (f(node, data)) {
			ret = 1;
			break;
		}

		for (j = 0; j < node->nout; j++) {
			dst = node->out[j].dst;
			assert(dst < graph->n);

			if (visited[dst])
				continue;

			darray_append(success, n, allocated, stack, dst);
			if (success) {
				ret = -1;
				goto out;
			}
		}
	}
 out:
	if (visitedallocated)
		free(visited);

	free(stack);

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

	graph->nodes = calloc(1, s);
	if (graph->nodes == NULL)
		return -1;

	graph->data = data;

	return 0;
}
