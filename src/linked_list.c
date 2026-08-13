#include "linked_list.h"

// static inline linked_list_node_t *linked_list_create_node_default(uint32_t value, uint32_t size)
// {
//     linked_list_node_t *n = malloc(size);

//     n->value = value;
//     n->right = NULL;
//     n->left = NULL;

//     return n;
// }

// linked_list_node_t *linked_list_create_root(uint32_t value, uint32_t value_size)
// {
//     return linked_list_create_node_default(value, value_size + sizeof(linked_list_node_t));
// }

// linked_list_node_t *linked_list_add(linked_list_node_t *left, uint32_t value, uint32_t value_size)
// {
//     linked_list_node_t *right = left->right;

//     linked_list_node_t *new = linked_list_create_node_default(value, value_size + sizeof(linked_list_node_t));

//     left->right = new;
//     new->left = left;

//     new->right = right;
//     if (right)
//     {
//         right->left = new;
//     }

//     return new;
// }

// void linked_list_del(linked_list_node_t *curr)
// {
//     linked_list_node_t *left = curr->left;
//     linked_list_node_t *right = curr->right;

//     if (left)
//     {
//         left->right = right;
//     }
//     if (right)
//     {
//         right->left = left;
//     }

//     free(curr);
// }
