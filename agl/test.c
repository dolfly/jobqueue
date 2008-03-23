#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "directedgraph.h"

int mydfs1(struct dgnode *node, void *data)
{
	assert(data == NULL);

	return 0;
}

int mydfs2(struct dgnode *node, void *data)
{
	assert(data == NULL);

	return (node->i == 2) ? 1 : 0;
}

int main(void)
{
	struct dgraph dg;
	size_t i, n;
	char visited[4];

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

	assert(agl_dfs(&dg, 0, NULL, mydfs1, NULL) == 0);

	/* Test incomplete DFS */
	assert(n == 4);
	memset(visited, 0, n);
	assert(agl_dfs(&dg, 0, visited, mydfs2, NULL) == 1);
	assert(visited[0] != 0 && visited[1] != 0 && visited[2] != 0 &&
	       visited[3] == 0);

	/* A deadlock test for DFS */
	if (agl_add_edge(&dg, 1, 0, NULL)) {
		fprintf(stderr, "Can not add feedback edge\n");
		exit(1);
	}
	assert(agl_dfs(&dg, 0, NULL, mydfs1, NULL) == 0);

	agl_deinit(&dg);

	return 0;
}
