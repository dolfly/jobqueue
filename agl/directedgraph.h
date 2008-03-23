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

/* dag_dfs() does a depth first search into the graph. No node is visited
 * twice. Only nodes that are reachable through edges from the initial node
 * are visited.
 *
 * parameters:
 *
 * graph:   Pointer to the graph
 * initial: Number of the DFS start node, in range [0, graph->n)
 * visited: Array of n characters, where n == graph->n. If given as NULL,
 *          it will not be possible to track DFS node visits directly.
 *          However, DFS node visits can be tracked with function f() and
 *          the data variable. On the first call, visited array must be
 *          initialized with zeros, otherwise not all nodes will be visited.
 * f:       Function f is called at each visited node. If function f returns
 *          a non-zero value, the search is terminated immediately. Otherwise
 *          the search is continued normally.
 * data:    A user-specified data pointer given to function f at each visit
 *          
 * Returns -1 if an error occured. If f() returns non-zero, 1 is returned.
 * Otherwise 0 is returned. In other words, 0 and 1 indicate a success,
 * but -1 indicates an error.
 */
int dag_dfs(struct dgraph *graph, size_t initial, char *visited,
	    int (*f)(struct dgnode *node, void *data), void *data);

/* dag_init() initializes a graph
 * 
 * parameters:
 * 
 * graph: Pointer to a struct dgraph that should be initialized
 * nodes: A hint about final number of nodes in the graph. This can be
 *        set to zero without any loss of flexibility.
 * data:  Arbitrary pointer given by the user. It can be used for any purpose.
 *
 * Returns 0 on success, -1 on failure.
 */
int dag_init(struct dgraph *graph, size_t nnodeshint, void *data);

#endif
