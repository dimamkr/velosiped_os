#ifndef DYNAMIC_ARRAY
#define DYNAMIC_ARRAY

#include "heap.h"
#include "types.h"
#include "system.h"
#include "random.h"

typedef struct
{
    byte_t *buffer;
    uint32_t size;
    uint32_t elements_count;
    uint32_t size_of_element;
    uint32_t start, end;
} dynamic_array_t;

typedef bool_t (*dynamic_array_less_cb) (void *a, void *b);

dynamic_array_t *dynamic_array_create(uint32_t size_of_element);
void dynamic_array_destroy(dynamic_array_t *array);
void dynamic_array_push_back(dynamic_array_t *array, void *element);
void dynamic_array_push_front(dynamic_array_t *array, void *element);
void dynamic_array_pop_back(dynamic_array_t *array);
void dynamic_array_pop_front(dynamic_array_t *array);
void dynamic_array_clear(dynamic_array_t *array);
void dynamic_array_quicksort(dynamic_array_t *array, uint32_t l_index, uint32_t r_index, dynamic_array_less_cb less);

#define dynamic_array_get_by_index(array, index) ((void *)((array)->buffer + (((array)->start + (index) + 1) & ((array)->size - 1)) * (array)->size_of_element))
#define dynamic_array_set_by_index(array, index, value) (memcpy(dynamic_array_get_by_index(array, index), value, (array)->size_of_element))
#define dynamic_array_get_bottom(array) dynamic_array_get_by_index(array, 0)
#define dynamic_array_get_top(array) ((void *)((array)->buffer + (((array)->end - 1) & ((array)->size - 1)) * (array)->size_of_element))

#endif