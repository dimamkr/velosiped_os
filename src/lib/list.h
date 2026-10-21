#ifndef LIST_H
#define LIST_H

// intrusive linked list

#include "types.h"
#include "pointer_utils.h"

typedef struct __list_node
{
    struct __list_node *left, *right;
} list_node_t;

static inline void list_init_root_cycle(list_node_t *root)
{
    root->left = root;
    root->right = root;
}

static inline void list_node_init(list_node_t *root)
{
    root->left = NULL;
    root->right = NULL;
}

static inline void list_erase(list_node_t **root_ptr, list_node_t *node)
{
    list_node_t *l = node->left;
    list_node_t *r = node->right;

    if (*root_ptr == node)
    {
        if ((*root_ptr)->left == *root_ptr)
        { /*единственный элемент*/
            *root_ptr = NULL;
            l = NULL;
            r = NULL;
        }
        else
        {
            *root_ptr = (*root_ptr)->right;
        }
    }

    if (l)
    {
        l->right = r;
    }
    if (r)
    {
        r->left = l;
    }

    node->left = NULL;
    node->right = NULL;
}

// удаление в циклическом списке без отслеживания корня
static inline void list_erase_cycle(list_node_t *node)
{
    if (node->left)
        node->left->right = node->right;
    if (node->right)
        node->right->left = node->left;
    node->left = NULL;
    node->right = NULL;
}

static inline void list_erase_move_root_left(list_node_t **root_ptr, list_node_t *node)
{
    list_node_t *l = node->left;
    list_node_t *r = node->right;

    if (*root_ptr == node)
    {
        if ((*root_ptr)->left == *root_ptr)
        { // единственный элемент
            *root_ptr = NULL;
            l = NULL;
            r = NULL;
        }
        else
        {
            *root_ptr = (*root_ptr)->left;
        }
    }

    if (l)
    {
        l->right = r;
    }
    if (r)
    {
        r->left = l;
    }

    node->left = NULL;
    node->right = NULL;
}

static inline void list_add(list_node_t *left, list_node_t *node)
{
    list_node_init(node);

    list_node_t *r = left->right;
    left->right = node;
    node->left = left;
    node->right = r;
    if (r)
    {
        r->left = node;
    }
}

// поддерживает *root = NULL
// поддерживает циклический связный список
static inline void list_add_begin(list_node_t **root_ptr, list_node_t *node)
{
    list_node_init(node);

    node->right = *root_ptr;
    if (*root_ptr)
    { /*зацикливание*/
        node->left = (*root_ptr)->left;
        if ((*root_ptr)->left)
        {
            (*root_ptr)->left->right = node;
        }
        (*root_ptr)->left = node;
    }
    *root_ptr = node;
}

#endif