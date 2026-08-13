#ifndef LINKED_LIST
#define LINKED_LIST

#include "types.h"
#include "heap.h"

typedef struct _linked_list_node
{
    void *value;
    struct _linked_list_node *left, *right;
} linked_list_node_t;

linked_list_node_t *linked_list_create_root(uint32_t value, uint32_t value_size);

linked_list_node_t *linked_list_add(linked_list_node_t *left, uint32_t value, uint32_t value_size);

void linked_list_del(linked_list_node_t *curr);

#endif
