#include "linked_list.h"

static inline linked_list_node_t *linked_list_create_node(void *value, uint32_t value_size)
{
    linked_list_node_t *n = malloc(sizeof(linked_list_node_t));
    n->right = NULL;
    n->left = NULL;
    n->value_size = value_size;

    n->value = malloc(value_size);
    memcpy(n->value, value, value_size);

    return n;
}

linked_list_node_t *linked_list_create_root(void *value, uint32_t value_size)
{
    return linked_list_create_node(value, value_size);
}

linked_list_node_t *linked_list_create_root_cycle(void *value, uint32_t value_size)
{
    linked_list_node_t *root = linked_list_create_node(value, value_size);
    root->right = root;
    root->left = root;
    return root;
}

linked_list_node_t *linked_list_add(linked_list_node_t *left, void *value, uint32_t value_size)
{
    linked_list_node_t *right = left->right;

    linked_list_node_t *new = linked_list_create_node(value, value_size);

    left->right = new;
    new->left = left;

    new->right = right;
    if (right)
    {
        right->left = new;
    }

    return new;
}

// поддерживает *root = NULL
// поддерживает циклический связный список
linked_list_node_t *linked_list_add_begin(linked_list_node_t **root, void *value, uint32_t value_size)
{
    linked_list_node_t *new = linked_list_create_node(value, value_size);

    new->right = *root;

    if (*root)
    {
        // зацикливание
        new->left = (*root)->left;

        if ((*root)->left)
        {
            (*root)->left->right = new;
        }

        (*root)->left = new;
    }

    *root = new;

    return new;
}

static inline void _linked_list_erase_end(linked_list_node_t *left, linked_list_node_t *right, linked_list_node_t **root, linked_list_node_t *curr)
{
    if (left)
    {
        left->right = right;
    }
    if (right)
    {
        right->left = left;
    }

    free(curr->value);
    free(curr);
}

void linked_list_erase(linked_list_node_t **root, linked_list_node_t *curr)
{
    linked_list_node_t *left = curr->left;
    linked_list_node_t *right = curr->right;

    if (*root == curr)
    {
        if ((*root)->right == *root)
        { // единственный элемент
            *root = NULL;
            left = NULL;
            right = NULL;
        }
        else
        {
            *root = (*root)->right;
        }
    }

    _linked_list_erase_end(left, right, root, curr);
}

void linked_list_erase_move_root_left(linked_list_node_t **root, linked_list_node_t *curr)
{
    linked_list_node_t *left = curr->left;
    linked_list_node_t *right = curr->right;

    if (*root == curr)
    {
        if ((*root)->left == *root)
        { // единственный элемент
            *root = NULL;
        }
        else
        {
            *root = (*root)->left;
        }
    }

    _linked_list_erase_end(left, right, root, curr);
}

void linked_list_destroy(linked_list_node_t **root)
{
    while (*root)
    {
        linked_list_erase(root, *root);
    }
}