#ifndef _DIRECTED_GRAPH_H_
#define _DIRECTED_GRAPH_H_

#include <stdio.h>

struct dgedge {
	size_t src;
	size_t dst;
	void *data;        /* Can be used by the application for any purpose */
};

struct dgnode {
	size_t i;             /* Node number in range [0, nnodes) */
	size_t nin;           /* Number of edges incoming */
	size_t nallocatedin;  /* Number of edges allocated for incoming */
	size_t nout;
	size_t nallocatedout;

	/* Outgoing edges */
	struct dgedge *out;

        /* Incoming edges: copies of outgoing edges of other nodes; that's
	   why only store pointers */
	struct dgedge **in;

	void *data;        /* Can be used by the application for any purpose */
};

struct dgraph {
	size_t n;
	size_t allocated;
	struct dgnode *nodes;

	void *data;        /* Can be used by the application for any purpose */
};

/* dag_add_edge() adds an edge from src node to dst node with a user-supplied
 * data pointer.
 *
 * Returns 0 on success, -1 otherwise.
 */
int dag_add_edge(struct dgraph *graph, size_t src, size_t dst, void *data);

/* dag_add_node() adds a node to the graph. data is a user-specified pointer
 * that can be used for any purpose.
 *
 * Returns 0 on success, -1 otherwise.
 */
int dag_add_node(struct dgraph *graph, void *data);

/* dag_create() creates a graph
 *
 * This function is same as dag_init() but the graph is allocated.
 * Returns a graph structure pointer on success and NULL on error.
 */
struct dgraph *dag_create(size_t nnodeshint, void *data);

/* dag_deinit() frees all resources allocated for the graph. However,
 * it does not free the given graph pointer. This call must be paired with
 * dag_init(), but not with dag_create().
 */
void dag_deinit(struct dgraph *graph);

/* dag_init() initializes a graph
 * 
 * parameters:
 * 
 * dag:   Pointer to a struct dgraph that should be initialized
 * nodes: A hint about final number of nodes in the graph. This can be
 *        set to zero without any loss of flexibility.
 * data:  Arbitrary pointer given by the user. It can be used for any purpose.
 *
 * Returns 0 on success, -1 on failure.
 */
int dag_init(struct dgraph *dag, size_t nnodeshint, void *data);

#endif
