#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "directedgraph.h"

int mydfs1(struct dgnode *node, void *data)
{
	assert(data == NULL);

	fprintf(stderr, "mydfs1: visit %zd\n", node->i);

	return 0;
}

int mydfs2(struct dgnode *node, void *data)
{
	assert(data == NULL);

	fprintf(stderr, "mydfs2: visit %zd\n", node->i);

	return (node->i == 2) ? 1 : 0;
}

int main(void)
{
	struct dgraph dg;
	size_t i, n;
	char visited[4];
	size_t fin[4];
	int cyclic;
	size_t *order;
	struct dgnode *node;
	struct dgedge *edge;
	size_t nedges;

	fprintf(stderr, "Directed graph test\n");

	if (agl_init(&dg, 0, NULL)) {
		fprintf(stderr, "No graph\n");
		exit(1);
	}

	n = 4;
	for (i = 0; i < n; i++) {
		if (agl_add_node(&dg, NULL)) {
			fprintf(stderr, "Can not add node %zd\n", i);
			exit(1);
		}
	}

	for (i = 0; i < (n - 1); i++) {
		if (agl_add_edge(&dg, i, i + 1, NULL)) {
			fprintf(stderr, "Can not add an edge to node %zd\n", i);
			exit(1);
		}
	}

	assert(agl_dfs(&dg, 0, NULL, NULL, mydfs1, NULL) == 0);

	/* Test incomplete DFS */
	assert(n == 4);
	memset(visited, 0, n);
	assert(agl_dfs(&dg, 0, visited, NULL, mydfs2, NULL) == 1);
	assert(visited[0] != 0 && visited[1] != 0 && visited[2] != 0 &&
	       visited[3] == 0);

	assert(!agl_has_cycles(&dg));

	/* A topological sort test */
	order = agl_topological_sort(&cyclic, &dg);
	assert(order != NULL);
	assert(order[0] == 0 && order[1] == 1 && order[2] == 2 &&
	       order[3] == 3);

	/* Test AGL_FOR_EACH_NODE() and AGL_FOR_EACH_EDGE() */
	memset(visited, 0, sizeof visited);
	nedges = 0;
	AGL_FOR_EACH_NODE(&dg, node) {
		visited[node->i] = 1;
		AGL_FOR_EACH_EDGE(node, edge)
			nedges++;
		AGL_END_FOR_EACH_EDGE();
	}
	AGL_END_FOR_EACH_NODE();
	for (i = 0; i < n; i++)
		assert(visited[i] != 0);
	assert(nedges == 3);

	/* A deadlock test for DFS */
	if (agl_add_edge(&dg, 1, 0, NULL)) {
		fprintf(stderr, "Can not add feedback edge\n");
		exit(1);
	}
	memset(visited, 0, n);
	assert(agl_dfs(&dg, 0, visited, fin, mydfs1, NULL) == 0);
	/* printf("%d %d %d %d\n", visited[0], visited[1], visited[2], visited[3]); */
	assert(visited[0] == 3);
	assert(visited[1] == 2 && visited[2] == 2 && visited[3] == 2);
	/* printf("%zd %zd %zd %zd\n", fin[0], fin[1], fin[2], fin[3]); */
	assert(fin[0] == 3 && fin[1] == 2 && fin[2] == 1 && fin[3] == 0);

	assert(agl_has_cycles(&dg));

	/* Another topological sort test */
	assert(agl_topological_sort(&cyclic, &dg) == NULL);
	assert(cyclic);

	agl_deinit(&dg);

	return 0;
}
