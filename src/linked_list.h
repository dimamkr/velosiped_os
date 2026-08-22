#ifndef LINKED_LIST
#define LINKED_LIST

#include "types.h"
#include "heap.h"
#include "system.h"

// TODO подкорректировать
// важно чтобы всегда узел имел одинаковый размер
typedef struct _linked_list_node
{
    struct _linked_list_node *left, *right;
    void *value;
    uint32_t value_size;
} linked_list_node_t;

linked_list_node_t *linked_list_create_root(void *value, uint32_t value_size);
linked_list_node_t *linked_list_create_root_cycle(void *value, uint32_t value_size);

linked_list_node_t *linked_list_add(linked_list_node_t *left, void *value, uint32_t value_size);

linked_list_node_t *linked_list_add_begin(linked_list_node_t **root, void *value, uint32_t value_size);

void linked_list_erase(linked_list_node_t **root, linked_list_node_t *curr);
// для циклического списка
void linked_list_erase_move_root_left(linked_list_node_t **root, linked_list_node_t *curr);

void linked_list_destroy(linked_list_node_t **root);

#endif
