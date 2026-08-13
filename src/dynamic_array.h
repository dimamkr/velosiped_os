#ifndef DYNAMIC_ARRAY
#define DYNAMIC_ARRAY

#include "heap.h"
#include "types.h"
#include "system.h"

typedef struct
{
    byte_t *buffer;
    uint32_t size;
    uint32_t elements_count;
    uint32_t size_of_element;
    uint32_t start, end;
} dynamic_array_t;

dynamic_array_t *dynamic_array_create(uint32_t size_of_element);
void dynamic_array_destroy(dynamic_array_t *array);
void array_push_back(dynamic_array_t *array, void *element);
void array_push_front(dynamic_array_t *array, void *element);
void array_pop_back(dynamic_array_t *array);
void array_pop_front(dynamic_array_t *array);

#define get_by_index(array, index) ((void*)((array)->buffer + (((array)->start + (index) + 1) & ((array)->size - 1)) * (array)->size_of_element))
#define get_bottom(array) get_by_index(array, 0)
#define get_top(array) ((void*)((array)->buffer + (((array)->end - 1) & ((array)->size - 1)) * (array)->size_of_element))

#endif