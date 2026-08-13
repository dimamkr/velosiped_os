#include "linked_list.h"

static inline linked_list_node_t *linked_list_create_node_default(void *value, uint32_t value_size)
{
    linked_list_node_t *n = malloc(value_size + sizeof(linked_list_node_t));

    memcpy(linked_list_node_get_value_ptr(n), value, value_size);
    n->right = NULL;
    n->left = NULL;

    return n;
}

linked_list_node_t *linked_list_create_root(void *value, uint32_t value_size)
{
    return linked_list_create_node_default(value, value_size);
}

linked_list_node_t *linked_list_add(linked_list_node_t *left, void *value, uint32_t value_size)
{
    linked_list_node_t *right = left->right;

    linked_list_node_t *new = linked_list_create_node_default(value, value_size);

    left->right = new;
    new->left = left;

    new->right = right;
    if (right)
    {
        right->left = new;
    }

    return new;
}

void *linked_list_node_get_value_ptr(linked_list_node_t *curr)
{
    return curr + 1;
}

linked_list_node_t *linked_list_add_begin(linked_list_node_t **root, void *value, uint32_t value_size)
{
    linked_list_node_t *new = linked_list_create_node_default(value, value_size);

    new->right = *root;
    *root = new;

    if (new->right)
    {
        new->right->left = new;
    }

    return new;
}

void linked_list_del(linked_list_node_t **root, linked_list_node_t *curr)
{
    linked_list_node_t *left = curr->left;
    linked_list_node_t *right = curr->right;

    if (*root == curr)
    {
        *root = (*root)->right;
    }

    if (left)
    {
        left->right = right;
    }
    if (right)
    {
        right->left = left;
    }

    free(curr);
}
