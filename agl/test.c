#include <stdlib.h>
#include <stdio.h>

#include "directedgraph.h"

int main(void)
{
	struct dgraph dg;
	size_t i, n;

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

	dag_deinit(&dg);

	return 0;
}
