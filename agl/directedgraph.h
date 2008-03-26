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

#define AGL_FOR_EACH_NODE(graph, node) do { \
	size_t _nodei; \
	for (_nodei = 0; _nodei < (graph)->n && (node = &(graph)->nodes[_nodei]) != NULL; _nodei++)

#define AGL_END_FOR_EACH_NODE() } while (0)

#define AGL_FOR_EACH_EDGE(node, edge) do { \
	size_t _edgei; \
	for (_edgei = 0; _edgei < (node)->nout && (edge = &(node)->out[_edgei]) != NULL; _edgei++)

#define AGL_END_FOR_EACH_EDGE() } while (0)

#define AGL_FOR_EACH_INCOMING_EDGE(node, edge) do { \
	size_t _edgei; \
	for (_edgei = 0; _edgei < (node)->nin && (edge = &(node)->in[_edgei]) != NULL; _edgei++)

#define AGL_END_FOR_EACH_INCOMING_EDGE() } while (0)

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

/* agl_b_levels() computes the b-level (or bottom level) value for each
 * node in the graph. b-level value of a node is the length of the longest path
 * from that node to an exit node. Exit node is a node that has no outgoing
 * edges. Length of a path is defined as a sum of node and edge weights
 * on the path.
 *
 * Parameters:
 *
 * graph:          Pointer to a graph
 * nf(node, data): nf() returns a node weight for a given node. If nf == NULL,
 *                 all node weights have value 1.0.
 * ef(edge, data): ef() returns an edge weight for a given edge. If ef == NULL,
 *                 all edge weights have value 0.0.
 * data:           A user-specified pointer given to both nf() and ef()
 *
 * Returns NULL on error, otherwise an array of b-level values. The array
 * length is exactly graph->n elements. It must be freed with free().
 */
double *agl_b_levels(struct dgraph *graph,
		     double (*nf)(struct dgnode *node, void *data),
		     double (*ef)(struct dgedge *node, void *data),
		     void *data);

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

/* agl_free() frees the graph allocated with agl_create()
 */
void agl_free(struct dgraph *graph);

/* agl_has_cycles() returns 1 if the graph has cycles, 0 if it doesn't have
 * cycles, and -1 on error. A cyclic graph has at least one node such that
 * there exists a path from that node back to itself.
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

/* agl_topological_sort() does a topological sort for a directed acyclil graph
 * ( http://en.wikipedia.org/wiki/Topological_sort ), and returns a
 * sorted array of node numbers that are in topological order. An error is
 * returned in if the graph is cyclic.
 * 
 * Topological order means that if node i is before node j in the
 * sorted array then node i is an ancestor of node j in the graph.
 * That means there is a directed path from i to j.
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
 * If graph->n == NULL, NULL is returned. The returned array must be
 * freed with free().
 */
size_t *agl_topological_sort(int *cyclic, struct dgraph *graph);

#endif
