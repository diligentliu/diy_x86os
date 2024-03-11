#include "tools/list.h"

void list_init(list_t *list) {
	list->first = (list_node_t *) 0;
	list->last = (list_node_t *) 0;
	list->count = 0;
}

void list_push_front(list_t *list, list_node_t *node) {
	node->next = list->first;
	node->prev = (list_node_t *) 0;
	if (list_is_empty(list)) {
		list->last = node;
	} else {
		list->first->prev = node;
	}
	list->first = node;
	list->count++;
}

void list_push_back(list_t *list, list_node_t *node) {
	node->prev = list->last;
	node->next = (list_node_t *) 0;
	if (list_is_empty(list)) {
		list->first = node;
	} else {
		list->last->next = node;
	}
	list->last = node;
	list->count++;
}

list_node_t *list_pop_front(list_t *list) {
	if (list_is_empty(list)) {
		return (list_node_t *) 0;
	}
	list_node_t *remove_node = list->first;
	list->first = remove_node->next;
	if (list->first == (list_node_t *) 0) {
		list->last = (list_node_t *) 0;
	} else {
		remove_node->next->prev = (list_node_t *) 0;
		// list->first->prev = (list_node_t *)0;
	}
	remove_node->next = (list_node_t *) 0;
	remove_node->prev = (list_node_t *) 0;
	list->count--;
	return remove_node;
}

list_node_t *list_ease(list_t *list, list_node_t *node) {
	if (node == list->first) {
		list->first = node->next;
	}

	if (node == list->last) {
		list->last = node->prev;
	}

	if (node->prev != (list_node_t *) 0) {
		node->prev->next = node->next;
	}

	if (node->next != (list_node_t *) 0) {
		node->next->prev = node->prev;
	}

	node->next = (list_node_t *) 0;
	node->prev = (list_node_t *) 0;
	list->count--;
	return node;
}