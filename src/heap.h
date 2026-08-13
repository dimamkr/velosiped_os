#ifndef HEAP
#define HEAP

#include "types.h"

#define HEAP_START_BLOCK 3 * MB
#define HEAP_END_BLOCK 100 * MB
#define HEAP_MAGIC 0x600DB10C

typedef volatile struct _heap_node
{
    uint32_t signature;
    uint32_t size;
    uint32_t start_addr;
    struct _heap_node *left;
    struct _heap_node *right;
} __attribute__((packed)) heap_node_t;

void *malloc(uint32_t size);
void free(void *ptr);
void heap_init();

uint32_t heap_memory_get_size_left();

#endif