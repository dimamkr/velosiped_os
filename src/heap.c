#include "heap.h"
#include "system.h"
#include "konsole.h"

heap_node_t *heap_root = NULL;
uint32_t heap_memory_size_left = HEAP_END_BLOCK - HEAP_START_BLOCK;

static inline heap_node_t *heap_node_create_default(uint32_t start_addr)
{
    heap_node_t *node = (heap_node_t *)start_addr;

    node->signature = HEAP_MAGIC;
    node->size = sizeof(heap_node_t);
    node->start_addr = start_addr;
    node->left = NULL;
    node->right = NULL;

    return node;
}

// изменяет лишь указатели на соседние блоки и start_addr у новосозданной
static inline heap_node_t *heap_add(heap_node_t *left, uint32_t start_addr)
{
    heap_node_t *right = left->right;

    heap_node_t *new = heap_node_create_default(start_addr);

    left->right = new;
    new->left = left;

    new->right = right;
    if (right)
    {
        right->left = new;
    }

    return new;
}

static inline void heap_del(heap_node_t *curr)
{
    heap_node_t *left = curr->left;
    heap_node_t *right = curr->right;

    if (left)
    {
        left->right = right;
    }
    if (right)
    {
        right->left = left;
    }
}

static inline heap_node_t *heap_process_first_fit_addr(uint32_t size)
{
    BARRIER;
    heap_node_t *n = heap_root;
    n = n->right;
    for (; n != NULL; n = n->right)
    {
        uint32_t end_of_left = n->left->start_addr + n->left->size;
        // konsole_printf("%s%x%s%x\n", "CHECK ", n->start_addr, " ", end_of_left);
        if (n->start_addr - end_of_left >= size)
        {

            heap_node_t *new = heap_add(n->left, end_of_left);
            new->size = size;

            heap_memory_size_left -= new->size;

            // konsole_printf("%s%x%s%x\n", "NEW ", new->start_addr, " ", new->start_addr + sizeof(heap_node_t));

            return new;
        }
    }

    PANIC("BAD MALLOC");
}

uint32_t heap_memory_get_size_left()
{
    return heap_memory_size_left;
}

void heap_init()
{
    heap_root = heap_node_create_default(HEAP_START_BLOCK);
    heap_add(heap_root, (uint32_t)HEAP_END_BLOCK);
}

void *malloc(uint32_t value_size)
{
    BARRIER;
    heap_node_t *node = heap_process_first_fit_addr(value_size + sizeof(heap_node_t));

    // konsole_printf("%s%x\n", "MALLOC PTR ", node->start_addr + sizeof(heap_node_t));
    return (void *)(node->start_addr + sizeof(heap_node_t));
}

void free(void *ptr)
{
    BARRIER;
    // konsole_printf("%s%x\n", "FREE PTR ", ptr);
    for (heap_node_t *n = heap_root; n != NULL; n = n->right)
    {
        // konsole_printf("%s%x\n", "CHECK PTR ", n->start_addr + sizeof(heap_node_t));
        if ((uint32_t)ptr == n->start_addr + sizeof(heap_node_t))
        {
            heap_memory_size_left += n->size;
            heap_del(n);
            return;
        }
    }

    PANIC("BAD FREE");
}
