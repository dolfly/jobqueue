#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "directedgraph.h"

int mydfs1(struct dgnode *node, void *data)
{
	assert(data == NULL);

	fprintf(stderr, "Visit node %zd\n", node->i);

	return 0;
}

int mydfs2(struct dgnode *node, void *data)
{
	assert(data == NULL);

	fprintf(stderr, "Visit node %zd\n", node->i);

	return (node->i == 2) ? 1 : 0;
}

int main(void)
{
	struct dgraph dg;
	size_t i, n;
	char visited[4];

	fprintf(stderr, "Directed graph test\n");

	if (dag_init(&dg, 0, NULL)) {
		fprintf(stderr, "No graph\n");
		exit(1);
	}

	n = 4;
	for (i = 0; i < n; i++) {
		if (dag_add_node(&dg, NULL)) {
			fprintf(stderr, "Can not add node %zd\n", i);
			exit(1);
		}
	}

	for (i = 0; i < (n - 1); i++) {
		if (dag_add_edge(&dg, i, i + 1, NULL)) {
			fprintf(stderr, "Can not add an edge to node %zd\n", i);
			exit(1);
		}
	}

	dag_dfs(&dg, 0, NULL, mydfs1, NULL);

	assert(n == 4);
	memset(visited, 0, n);
	dag_dfs(&dg, 0, visited, mydfs2, NULL);

	assert(visited[0] != 0 && visited[1] != 0 && visited[2] != 0 &&
	       visited[3] == 0);

	dag_deinit(&dg);

	return 0;
}
