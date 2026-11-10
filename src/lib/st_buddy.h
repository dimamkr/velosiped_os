#ifndef ST_BUDDY_H
#define ST_BUDDY_H

#include "types.h"

// узел дерева segment tree buddy allocator
// free_log2p1 = log2(размер наибольшего свободного блока в поддереве) + 1,
// тажке 0 <=> в поддереве нет свободного блока.
// f_a >= f_b <=> max_free_a >= max_free_b,
typedef struct
{
    uint8_t free_log2p1;
} st_buddy_node_t;

typedef struct
{
    st_buddy_node_t *tree;
    size_t depth;
    size_t leaves;    // кол-во листьев
    size_t tree_size; // размер массива
    bool_t owns_tree; // false для init_from_buff: destroy не освобождает tree
} st_buddy_t;

// ошибка специфическая, тк есть 0 адрес
#define ST_BUDDY_ERROR ((size_t)-1)

st_buddy_t *st_buddy_create(size_t depth);
void st_buddy_init(st_buddy_t *this, size_t depth);
void st_buddy_init_from_buff(st_buddy_t *this, void *buff, size_t depth);
void st_buddy_destroy(st_buddy_t *this);
size_t st_buddy_alloc(st_buddy_t *this, size_t size);
bool_t st_buddy_free(st_buddy_t *this, size_t addr);
bool_t st_buddy_is_free(st_buddy_t *this, size_t addr);

bool_t st_buddy_set_alloced(st_buddy_t *this, size_t laddr, size_t length);

static inline size_t st_buddy_capacity(st_buddy_t *this)
{
    return this->leaves;
}

#endif