#ifndef _VPLIST_H_
#define _VPLIST_H_

#include <stdio.h>

struct vplist;

struct vplist {
	struct vplist *next;
	void *item;
};

int vplist_append(struct vplist *v, void *item);
struct vplist *vplist_create(void);
void vplist_free_items(struct vplist *v);
void *vplist_get(const struct vplist *v, size_t i);
void vplist_init(struct vplist *v);
size_t vplist_len(const struct vplist *v);
void *vplist_pop_head(struct vplist *v);
void *vplist_pop_tail(struct vplist *v);

#endif
