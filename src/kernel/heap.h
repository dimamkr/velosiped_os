#ifndef HEAP
#define HEAP

#include "types.h"
#include "system.h"

#define HEAP_DATA_BLOCK_MAGIC 0x600DB10C
#define HEAP_VOID_BLOCK_MAGIC 0xDEFEC8ED

// ВНИМАНИЕ ВАЖНО ЧТОБЫ sizeof от обеих структур был одинаков
// ТАКЖЕ ВАЖНО ЧТОБЫ СИГНАТУРЫ И РАЗМЕРЫ ХРАНИЛИСЬ В ОДНОМ МЕСТЕ

// далее пустотные данные и размер, сигнатура
typedef struct _heap_void_node
{
    uint32_t signature;
    uint32_t size; // размер именно пустоты
    struct _heap_void_node *left;
    struct _heap_void_node *right;
} __attribute__((packed)) heap_void_block_t;

// далее выделенные данные и размер, сигнатура
typedef struct
{
    uint32_t signature;
    uint32_t size; // размер именно данных
    uint32_t dummy1;
    uint32_t dummy2; // пустышки нужны чтобы на месте каждого блока данных мог инициализироваться пустотный узел
} __attribute__((packed)) heap_data_block_t;

typedef struct
{
    uint32_t size;
    uint32_t signature;
} __attribute__((packed)) heap_end_block_t;

void *malloc(uint32_t size);
void *alligned_malloc(uint32_t size, uint32_t alignment);
void *realloc(void *ptr, uint32_t size);
void free(void *ptr);
void heap_init();

uint32_t heap_memory_get_size_left();

#endif