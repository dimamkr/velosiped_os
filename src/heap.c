#include "heap.h"
#include "ram.h"

// segregated lists heap
// инварианты:
// не существует двух соседних пустотных блоков
// malloc всегда из начала пустоты

// TODO
// задуматься об утечке памяти которая может быть из-за увеличения размера защитных блоков по бокам
// TODO
// рефакторинг

#define HEAP_VOID_BUCKET_COUNT 30
// uint32_t heap_void_bucket_size[HEAP_VOID_BUCKET_COUNT];

// i-й элемент это указатель на корень с соотв разбросом размеров
heap_void_block_t *heap_void_bucket_root[HEAP_VOID_BUCKET_COUNT];

#define LEFT_EDGE_SIZE (sizeof(heap_void_block_t))
#define RIGHT_EDGE_SIZE (sizeof(heap_end_block_t))
#define SERVICE_FIELDS_SIZE (LEFT_EDGE_SIZE + RIGHT_EDGE_SIZE)

// наим степень двойки не меньшая числа
static inline uint32_t get_void_block_size_index(uint32_t size)
{
    if (size <= 1)
        return 0;
    // __builtin_clz возвращает число ведущих нулей в 32-битном числе..
    return (32 - __builtin_clz(size - 1));
}

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

        heap_void_bucket_erase(heap_void_bucket_root + get_void_block_size_index(((heap_void_block_t *)left_start_addr)->size),
                               (heap_void_block_t *)left_start_addr);

        new_start_addr = left_start_addr;
    }

    // первый после конца
    heap_void_block_t *right_start_block = (heap_void_block_t *)get_block_end(start_addr);

    if (right_start_block->signature == HEAP_VOID_BLOCK_MAGIC)
    {
        found_neighbours = true;

        heap_void_bucket_erase(heap_void_bucket_root + get_void_block_size_index(right_start_block->size), right_start_block);

        new_end_addr = get_block_end((byte_t *)right_start_block);
    }

    if (found_neighbours)
    {
        heap_void_bucket_erase(heap_void_bucket_root + get_void_block_size_index(void_block->size), void_block);

        uint32_t new_size = new_end_addr - new_start_addr - SERVICE_FIELDS_SIZE;

        create_void_block_default(new_start_addr, new_size);
        heap_void_bucket_add_begin(heap_void_bucket_root + get_void_block_size_index(((heap_void_block_t *)new_start_addr)->size),
                                   (heap_void_block_t *)new_start_addr);
    }
}

void heap_init()
{
    uint32_t heap_end_block = KHEAP_END - SERVICE_FIELDS_SIZE;
    // вспомогательные блоки для верной навигации по соседним
    create_data_block((byte_t *)KHEAP_START, 0);
    create_data_block((byte_t *)heap_end_block, 0);

    for (uint32_t i = 0; i < HEAP_VOID_BUCKET_COUNT; ++i)
    {
        heap_void_bucket_root[i] = NULL;
    }

    byte_t *void_block_start = get_block_end((byte_t *)KHEAP_START);

    create_void_block_default(void_block_start, (uint32_t)(heap_end_block - (uint32_t)void_block_start - SERVICE_FIELDS_SIZE));
    heap_void_bucket_add_begin(heap_void_bucket_root + get_void_block_size_index(((heap_void_block_t *)void_block_start)->size),
                               (heap_void_block_t *)void_block_start);
}

static inline byte_t *try_malloc_from_bucket(heap_void_block_t *n, uint32_t index, uint32_t size)
{
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
                heap_void_bucket_erase(heap_void_bucket_root + index, n);
                create_data_block((byte_t *)n, size);
            }
            else
            {
                heap_void_bucket_erase(heap_void_bucket_root + index, n);
                create_data_block((byte_t *)n, size);

                // указывает на начало нового блока пустоты
                heap_void_block_t *new = (heap_void_block_t *)get_block_end((byte_t *)n);
                create_void_block_default((byte_t *)new, void_size_left);

                heap_void_bucket_add_begin(heap_void_bucket_root + get_void_block_size_index(new->size), new);
            }

            // n - это теперь адрес начала данных
            return get_data_ptr((byte_t *)n);
        }

        n = n->right;
    }

    return NULL;
}

void *malloc(uint32_t size)
{
    if (size == 0)
    {
        return NULL;
    }

    uint32_t index = get_void_block_size_index(size);

    if (index >= HEAP_VOID_BUCKET_COUNT)
    {
        PANIC("BAD MALLOC SIZE");
    }

    // пытаемся зайти в пустоту с большим размером (начиная с наименьшего)
    for (uint32_t i = index; i < HEAP_VOID_BUCKET_COUNT; ++i)
    {
        byte_t *ptr = try_malloc_from_bucket(heap_void_bucket_root[i], i, size);
        if (ptr)
        {
            return ptr;
        }
    }

    if (index > 0)
    {
        // если нет, то берем в области где в теории может быть подходящий размер (редкое событие)
        byte_t *ptr = try_malloc_from_bucket(heap_void_bucket_root[index - 1], index - 1, size);
        if (ptr)
        {
            return ptr;
        }
    }

    PANIC("BAD MALLOC");
}

void *alligned_malloc(uint32_t size, uint32_t alignment)
{
    if (size == 0 || alignment == 0)
    {
        return NULL;
    }

    // не степень двойки
    if (alignment & (alignment - 1))
    {
        PANIC("BAD ALLIGNMENT");
    }

    byte_t *_ptr = malloc(size + alignment - 1);

    if (!_ptr)
    {
        return NULL;
    }

    // также начало выделеннного блока
    byte_t *prev_end = _ptr - LEFT_EDGE_SIZE;
    // первое не большее кратное выравниванию (степени двойки)
    byte_t *alligned_ptr = (byte_t *)(((uint32_t)_ptr + alignment - 1) & ~(alignment - 1));
    byte_t *curr_start = alligned_ptr - LEFT_EDGE_SIZE;

    heap_block_erase(prev_end);
    // TODO расширение блока пустоты справа, если он там есть
    create_data_block(curr_start, get_block_end(prev_end) - curr_start - SERVICE_FIELDS_SIZE);

    // слева не может быть блок пустоты
    if (((heap_end_block_t *)(prev_end - RIGHT_EDGE_SIZE))->signature != HEAP_DATA_BLOCK_MAGIC)
    {
        PANIC("BAD ALLIGNED_ALLOC");
    }

    byte_t *prev_start = get_block_start(prev_end);

    // сколько места останется на новый пустотный узел
    int void_size_left = (curr_start - prev_end) - SERVICE_FIELDS_SIZE;

    // если не поместится следующий нетривиальный пустотный узел
    if (void_size_left <= 0)
    {
        uint32_t old_size = ((heap_data_block_t *)prev_start)->size;
        heap_block_erase(prev_start);

        create_data_block(prev_start, old_size + curr_start - prev_end);
    }
    else
    {
        create_void_block_default(prev_end, void_size_left);
        heap_void_bucket_add_begin(heap_void_bucket_root + get_void_block_size_index(((heap_void_block_t *)prev_end)->size), (heap_void_block_t *)prev_end);
    }

    return alligned_ptr;
}

void free(void *ptr)
{
    if (!ptr)
        return;

    // начало блока данных
    byte_t *start = (byte_t *)ptr - LEFT_EDGE_SIZE;

    if (((heap_data_block_t *)start)->signature != HEAP_DATA_BLOCK_MAGIC)
    {
        if (((heap_data_block_t *)start)->signature == HEAP_VOID_BLOCK_MAGIC)
            PANIC("DOUBLE FREE");
        else
            PANIC("BAD FREE");
    }

    heap_block_erase(start);

    // места на новый пустотный узел гарантированно хватает
    create_void_block_default(start, ((heap_data_block_t *)start)->size);

    heap_void_bucket_add_begin(heap_void_bucket_root + get_void_block_size_index(((heap_void_block_t *)start)->size), (heap_void_block_t *)start);
    heap_void_block_try_merge((heap_void_block_t *)start);
}

void *realloc(void *ptr, uint32_t size)
{
    if (ptr == NULL)
        return malloc(size);
    if (size == 0)
    {
        free(ptr);
        return NULL;
    }

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
            heap_void_bucket_erase(heap_void_bucket_root + get_void_block_size_index(next->size), next);
            create_data_block((byte_t *)data_block, size);
        }
        else
        {
            heap_void_bucket_erase(heap_void_bucket_root + get_void_block_size_index(next->size), next);
            create_data_block((byte_t *)data_block, size);

            // указывает на начало нового блока пустоты
            heap_void_block_t *new = (heap_void_block_t *)get_block_end((byte_t *)data_block);
            create_void_block_default((byte_t *)new, void_size_left);

            heap_void_bucket_add_begin(heap_void_bucket_root + get_void_block_size_index(new->size), new);
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
