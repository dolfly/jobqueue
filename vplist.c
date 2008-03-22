#include <assert.h>
#include <stdlib.h>

#include "vplist.h"


/* Make a new tail. O(n) operation. Returns 0 on success, -1 on failure. */
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


/* Return element i from the list (i == 0 is the head). Return NULL if
 * i is too high (i >= vplist_len()).
 */
void *vplist_get(const struct vplist *v, size_t i)
{
	size_t j;

	for (j = 0; j <= i && v != NULL; j++)
		v = v->next;

	if (v == NULL)
		return NULL;

	return v->item;
}


/* Init an existing list head */
void vplist_init(struct vplist *v)
{
	v->next = NULL;
	v->item = NULL;
}

/* Returns 1 if the list is empty, otherwise 0 */
int vplist_is_empty(const struct vplist *v)
{
	return v->next == NULL;
}

/* Return the number of elements in list */
size_t vplist_len(const struct vplist *v)
{
	size_t l = 0;

	while (v->next != NULL) {
		v = v->next;
		l++;
	}

	return l;
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

/* Remove node that matches item in the list. Returns 0 on success, -1 on
 * failure. */
int vplist_remove_item(struct vplist *v, void *item)
{
	void *removeditem;

	/* Find the node that precedes the node that is being searched for */
	while (v->next != NULL && v->next->item != item)
		v = v->next;

	if (v->next == NULL)
		return -1;

	removeditem = vplist_pop_head(v);
	assert(removeditem == item);

	return 0;
}
