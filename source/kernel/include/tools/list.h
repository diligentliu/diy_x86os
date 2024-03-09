#ifndef OS_LIST_H
#define OS_LIST_H

#include "comm/types.h"

typedef struct _list_node_t {
	struct _list_node_t *prev;
	struct _list_node_t *next;
} list_node_t;

static inline void list_node_init(list_node_t *node) {
	node->next = node;
	node->prev = node;
}

static inline list_node_t *list_node_pre(list_node_t *node) {
	return node->prev;
}

static inline list_node_t *list_node_next(list_node_t *node) {
	return node->next;
}

typedef struct _list_t {
	list_node_t *first;
	list_node_t *last;
	uint32_t count;
} list_t;

void list_init(list_t *list);

static inline int list_is_empty(list_t *list) {
	return list->count == 0;
}

static inline int list_count(list_t *list) {
	return list->count;
}

static inline list_node_t *list_first(list_t *list) {
	return list->first;
}

static inline list_node_t *list_last(list_t *list) {
	return list->last;
}

void list_push_front(list_t *list, list_node_t *node);
void list_push_back(list_t *list, list_node_t *node);
list_node_t *list_pop_front(list_t *list);
list_node_t *list_ease(list_t *list, list_node_t *node);

#define parent_addr(member_ptr, parent_type, member) \
	((parent_type *)((uint32_t)member_ptr - (uint32_t)&((parent_type *)0)->member))

#define list_node_parent(node, parent_type, member) \
	(parent_type *) (node ? parent_addr(node, parent_type, member) : 0)

#endif //OS_LIST_H
