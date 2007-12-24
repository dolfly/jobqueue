#include <assert.h>
#include <stdlib.h>

#include "vplist.h"


/* Make a new tail. O(n) operation. */
int vplist_append(struct vplist *v, void *item)
{
	assert(item != NULL);

	while (v->next != NULL)
		v = v->next;

	v->next = malloc(sizeof v[0]);
	if (v->next == NULL)
		return -1;

	*v->next = (struct vplist) {.item = item};

	return 0;
}


/* Allocate a new head */
struct vplist *vplist_create(void)
{
	struct vplist *v = malloc(sizeof v[0]);
	*v = (struct vplist) {.next = NULL};
	return v;
}


/* Free all items in the list. O(n) operation. */
void vplist_free_items(struct vplist *v)
{
	void *item;

	do {
		item = vplist_pop_head(v);
		free(item);
	} while (item != NULL);
}


/* Init an existing list head */
void vplist_init(struct vplist *v)
{
	v->next = NULL;
	v->item = NULL;
}


/* Pop head of the list. O(1) operation. */
void *vplist_pop_head(struct vplist *v)
{
	void *item;
	struct vplist *next = v->next;

	if (next == NULL)
		return NULL;

	item = next->item;
	assert(item != NULL);

	v->next = next->next;

	/* Prevent damage */
	next->next = NULL;
	next->item = NULL;

	free(next);

	return item;
}


/* Pop tail of the list. O(n) operation. */
void *vplist_pop_tail(struct vplist *v)
{
	if (v->next == NULL)
		return NULL;

	while (v->next->next != NULL)
		v = v->next;

	return vplist_pop_head(v);
}
