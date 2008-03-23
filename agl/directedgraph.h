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

/* agl_add_edge() adds an edge from src node to dst node with a user-supplied
 * data pointer.
 *
 * Returns 0 on success, -1 otherwise.
 */
int agl_add_edge(struct dgraph *graph, size_t src, size_t dst, void *data);

/* agl_add_node() adds a node to the graph. data is a user-specified pointer
 * that can be used for any purpose.
 *
 * Returns 0 on success, -1 otherwise.
 */
int agl_add_node(struct dgraph *graph, void *data);

/* agl_create() creates a graph
 *
 * This function is same as agl_init() but the graph is allocated.
 * Returns a graph structure pointer on success and NULL on error.
 */
struct dgraph *agl_create(size_t nnodeshint, void *data);

/* agl_deinit() frees all resources allocated for the graph. However,
 * it does not free the given graph pointer. This call must be paired with
 * agl_init(), but not with agl_create().
 */
void agl_deinit(struct dgraph *graph);

/* agl_dfs() does a depth first search into the graph. No node is visited
 * twice. Only nodes that are reachable through edges from the initial node
 * are visited.
 *
 * parameters:
 *
 * graph:   Pointer to the graph
 * initial: Number of the DFS start node, in range [0, graph->n)
 * visited: Array of graph->n characters. If visited == NULL,
 *          it will not be possible to track DFS node visits directly.
 *          However, DFS node visits can be tracked with function f().
 *          On the first call to agl_dfs(), visited must be
 *          initialized with zeros, otherwise not all nodes will be visited.
 *          If a cycle was detected in the graph, there is a value 3 in the
 *          visited array.
 * fin:     If fin != NULL, fin[i] determines the iteration number
 *          at which node i was left (dropped of the stack). fin array
 *          must have exactly graph->n size_t elements.
 * f:       Function f is called at each visited node. If function f returns
 *          a non-zero value, the search is terminated immediately. Otherwise
 *          the search is continued normally. f may be NULL in which case
 *          nothing is called.
 * data:    A user-specified data pointer given to function f at each visit
 *          
 * Returns -1 if an error occured. If f() returns non-zero, 1 is returned.
 * Otherwise 0 is returned. In other words, 0 and 1 indicate a success,
 * but -1 indicates an error.
 */
int agl_dfs(struct dgraph *graph, size_t initial, char *visited, size_t *fin,
	    int (*f)(struct dgnode *node, void *data), void *data);

/* agl_has_cycles() returns 1 if the graph has cycles, 0 if it doesn't have
 * cycles, and -1 on error
 */
int agl_has_cycles(struct dgraph *graph);

/* agl_init() initializes a graph
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
int agl_init(struct dgraph *graph, size_t nnodeshint, void *data);

/* agl_topological_sort() does a topological sort for the graph:
 * http://en.wikipedia.org/wiki/Topological_sort
 *
 * This one also detects cycles.
 *
 * Parameters:
 *
 * cyclic: Set to 1 if the graph has cycles (topological sort is meaningless
 *         in this case)
 * graph:  Pointer to the graph
 *
 * Returns NULL on error (out of memory (*cycles == 0) or the graph has
 * cycles (*cycles == 1)). Otherwise, return a pointer to a graph->n
 * element array containing node numbers in topologically sorted order.
 * If graph->n == NULL, NULL is returned.
 */
size_t *agl_topological_sort(int *cyclic, struct dgraph *graph);

#endif
