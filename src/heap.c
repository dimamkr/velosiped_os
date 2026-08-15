#include "heap.h"

// TODO
// реализовать полную версию segregated lists где какое угодное количество HEAP_VOID_BUCKET_COUNT
#define HEAP_VOID_BUCKET_COUNT 1
uint32_t heap_void_bucket_size[HEAP_VOID_BUCKET_COUNT];

// i-й элемент это указатель на корень с соотв разбросом размеров
heap_void_block_t *heap_void_bucket_root[HEAP_VOID_BUCKET_COUNT];

#define RIGHT_EDGE_SIZE (2 * sizeof(uint32_t))
#define SERVICE_FIELDS_SIZE (sizeof(heap_void_block_t) + RIGHT_EDGE_SIZE)
#define LEFT_EDGE_SIZE (sizeof(heap_void_block_t))

static inline byte_t *get_data_ptr(byte_t *block_start_addr)
{
    return block_start_addr + LEFT_EDGE_SIZE;
}

static inline byte_t *get_block_start(byte_t *block_end_addr)
{
    uint32_t size = ((heap_end_block_t *)(block_end_addr - RIGHT_EDGE_SIZE))->size;
    return block_end_addr - SERVICE_FIELDS_SIZE - size;
}

static inline byte_t *get_block_end(byte_t *block_start_addr)
{
    // типы данных блоков можно спокойно перегонять друг в друга для нахождения размера например
    uint32_t size = ((heap_void_block_t *)block_start_addr)->size;
    return block_start_addr + SERVICE_FIELDS_SIZE + size;
}

static inline void create_data_block(byte_t *start_addr, uint32_t size)
{
    heap_data_block_t *_start_addr = (heap_data_block_t *)start_addr;
    _start_addr->signature = HEAP_DATA_BLOCK_MAGIC;
    _start_addr->size = size;

    start_addr = get_data_ptr(start_addr) + size;
    ((heap_end_block_t *)start_addr)->size = size;
    ((heap_end_block_t *)start_addr)->signature = HEAP_DATA_BLOCK_MAGIC;
}

static inline void create_void_block_default(byte_t *start_addr, uint32_t size)
{
    heap_void_block_t *_start_addr = (heap_void_block_t *)start_addr;
    _start_addr->signature = HEAP_VOID_BLOCK_MAGIC;
    _start_addr->size = size;

    _start_addr->left = NULL;
    _start_addr->right = NULL;

    start_addr = get_data_ptr(start_addr) + size;
    ((heap_end_block_t *)start_addr)->size = size;
    ((heap_end_block_t *)start_addr)->signature = HEAP_VOID_BLOCK_MAGIC;
}

// стираем сигнатуры
static inline void heap_block_erase(byte_t *start_addr)
{
    ((heap_void_block_t *)start_addr)->signature = 0x0;
    ((heap_void_block_t *)(start_addr + ((heap_void_block_t *)start_addr)->size + LEFT_EDGE_SIZE))->signature = 0x0;
}

static inline void heap_void_bucket_erase(heap_void_block_t **root, heap_void_block_t *curr)
{
    heap_void_block_t *right = curr->right;
    heap_void_block_t *left = curr->left;

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

    heap_block_erase((byte_t *)curr);
}

// добавление уже инциализированного пустотного узла
static inline void heap_void_bucket_add_begin(heap_void_block_t **root, heap_void_block_t *new)
{
    new->right = *root;
    *root = new;

    if (new->right)
    {
        new->right->left = new;
    }
}

// объединение узла с соседними (по адресам) пустотными (если такие есть)
static inline void heap_void_block_try_merge(heap_void_block_t *void_block)
{
    byte_t *start_addr = (byte_t *)void_block;

    byte_t *new_start_addr = start_addr;
    byte_t *new_end_addr = get_block_end(start_addr);
    bool found_neighbours = false;

    // первый за началом
    heap_end_block_t *left_end_block = (heap_end_block_t *)(start_addr - RIGHT_EDGE_SIZE);

    if (left_end_block->signature == HEAP_VOID_BLOCK_MAGIC)
    {
        found_neighbours = true;

        byte_t *left_start_addr = get_block_start(start_addr);

        heap_void_bucket_erase(heap_void_bucket_root + 0, (heap_void_block_t *)left_start_addr);

        new_start_addr = left_start_addr;
    }

    // первый после конца
    heap_void_block_t *right_start_block = (heap_void_block_t *)get_block_end(start_addr);

    if (right_start_block->signature == HEAP_VOID_BLOCK_MAGIC)
    {
        found_neighbours = true;

        heap_void_bucket_erase(heap_void_bucket_root + 0, right_start_block);

        new_end_addr = get_block_end((byte_t *)right_start_block);
    }

    if (found_neighbours)
    {
        heap_void_bucket_erase(heap_void_bucket_root + 0, void_block);

        uint32_t new_size = new_end_addr - new_start_addr - SERVICE_FIELDS_SIZE;

        create_void_block_default(new_start_addr, new_size);
        heap_void_bucket_add_begin(heap_void_bucket_root + 0, (heap_void_block_t *)new_start_addr);
    }
}

void heap_init()
{
    // вспомогательные блоки для верной навигации по соседним
    create_data_block((byte_t *)HEAP_START_BLOCK, 0);
    create_data_block((byte_t *)HEAP_END_BLOCK, 0);

    heap_void_bucket_root[0] = NULL;

    byte_t *void_block_start = (byte_t *)HEAP_START_BLOCK;

    create_void_block_default(void_block_start, (uint32_t)(HEAP_END_BLOCK - HEAP_START_BLOCK - SERVICE_FIELDS_SIZE));
    heap_void_bucket_add_begin(heap_void_bucket_root + 0, (heap_void_block_t *)void_block_start);
}

void *malloc(uint32_t size)
{
    // TODO сделать нормальный выбор подходящего размера
    heap_void_block_t *n = heap_void_bucket_root[0];
    while (n)
    {
        if (n->size >= size)
        {
            // сколько места останется на новый пустотный узел
            int void_size_left = n->size - size - SERVICE_FIELDS_SIZE;

            // если не поместится следующий нетривиальный пустотный узел
            if (void_size_left <= 0)
            {
                // то отдаем сразу всю память пустотного узла
                size = n->size;
                heap_void_bucket_erase(heap_void_bucket_root + 0, n);
                create_data_block((byte_t *)n, size);
            }
            else
            {
                heap_void_bucket_erase(heap_void_bucket_root + 0, n);
                create_data_block((byte_t *)n, size);

                // указывает на начало нового блока пустоты
                heap_void_block_t *new = (heap_void_block_t *)get_block_end((byte_t *)n);
                create_void_block_default((byte_t *)new, void_size_left);

                heap_void_bucket_add_begin(heap_void_bucket_root + 0, new);
            }

            // n - это теперь адрес начала данных
            return get_data_ptr((byte_t *)n);
        }

        n = n->right;
    }

    PANIC("BAD MALLOC");
}

void free(void *ptr)
{
    if (!ptr)
        return;

    // начало блока данных
    byte_t *start = (byte_t *)ptr - LEFT_EDGE_SIZE;

    if (((heap_data_block_t *)start)->signature != HEAP_DATA_BLOCK_MAGIC)
    {
        PANIC("BAD FREE");
    }

    heap_block_erase(start);

    // места на новый пустотный узел гарантированно хватает
    create_void_block_default(start, ((heap_data_block_t *)start)->size);

    heap_void_bucket_add_begin(heap_void_bucket_root, (heap_void_block_t *)start);
    heap_void_block_try_merge((heap_void_block_t *)start);
}

void *realloc(void *ptr, uint32_t size)
{
    if (ptr == NULL)
        return malloc(size);

    heap_data_block_t *data_block = (heap_data_block_t *)((byte_t *)ptr - LEFT_EDGE_SIZE);

    if (data_block->signature != HEAP_DATA_BLOCK_MAGIC)
        PANIC("BAD REALLOC");

    // могли отдать остатки пустотного узла
    if (data_block->size >= size)
    {
        return ptr;
    }

    uint32_t old_size = data_block->size;

    heap_void_block_t *next = (heap_void_block_t *)get_block_end((byte_t *)data_block);

    void *new_ptr = NULL;
    if (next->signature == HEAP_VOID_BLOCK_MAGIC && next->size + data_block->size + SERVICE_FIELDS_SIZE >= size)
    {
        int void_size_left = next->size + data_block->size - size;

        // если не поместится следующий нетривиальный пустотный узел
        if (void_size_left <= 0)
        {
            // то отдаем сразу всю память пустотного узла
            size = next->size + data_block->size + SERVICE_FIELDS_SIZE;
            heap_void_bucket_erase(heap_void_bucket_root + 0, next);
            create_data_block((byte_t *)data_block, size);
        }
        else
        {
            heap_void_bucket_erase(heap_void_bucket_root + 0, next);
            create_data_block((byte_t *)data_block, size);

            // указывает на начало нового блока пустоты
            heap_void_block_t *new = (heap_void_block_t *)get_block_end((byte_t *)data_block);
            create_void_block_default((byte_t *)new, void_size_left);

            heap_void_bucket_add_begin(heap_void_bucket_root + 0, new);
        }

        new_ptr = get_data_ptr((byte_t *)data_block);
    }
    else
    {
        new_ptr = malloc(size);
        memcpy(new_ptr, ptr, old_size);
        free(ptr);
    }

    return new_ptr;
}
