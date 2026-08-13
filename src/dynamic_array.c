#include "dynamic_array.h"


dynamic_array_t *dynamic_array_create(uint32_t size_of_element)
{
    dynamic_array_t *array = malloc(sizeof(dynamic_array_t));

    array->start = 0;
    array->end = 0;
    array->size = 1;
    array->size_of_element = size_of_element;
    array->buffer = malloc(size_of_element);
    array->elements_count = 0;
    
    return array;
}


void dynamic_array_destroy(dynamic_array_t *array)
{
    free(array->buffer);
    free(array);
}


static void array_expand_(dynamic_array_t *array)
{
    if (array->size != array->elements_count)
        return;

    array->buffer = realloc(array->buffer, 2 * array->size * array->size_of_element);

    if (array->size == 1)
    {
        array->start = 1;
        array->end = 1;
    }
    else if (array->start < array->end)
    {
        uint32_t right_elements_count = array->size - array->start - 1;

        memcpy(array->buffer + (2 * array->size - right_elements_count) * array->size_of_element,
            array->buffer + (array->start + 1) * array->size_of_element,
            right_elements_count * array->size_of_element);

        array->start = 2 * array->size - right_elements_count - 1;
    }
    else
    {
        array->start = 2 * array->size - 1;
        array->end = array->elements_count;
    }
    
    array->size *= 2;
}


static void array_shrink_(dynamic_array_t *array)
{
    if (array->elements_count == 0 && array->size != 1)
    {
        realloc(array->buffer, array->size_of_element);
        array->size = 1;
        array->start = 0;
        array->end = 0;
        
        return;
    }

    uint32_t new_size = 1 << (32 - __builtin_clz(array->elements_count - 1));

    if (new_size == array->size)
        return;

    if (array->start < array->end || array->end == 0)
    {
        memcpy(array->buffer,
            array->buffer + (array->start + 1) * array->size_of_element,
            array->elements_count * array->size_of_element);

        array->start = new_size - 1;
        array->end = array->elements_count & (new_size - 1);
    }
    else if (array->start > array->end)
    {
        uint32_t right_elements_count = array->size - array->start - 1;

        memcpy(array->buffer + (new_size - right_elements_count) * array->size_of_element,
            array->buffer + (array->start + 1) * array->size_of_element,
            right_elements_count * array->size_of_element);

        array->start = new_size - right_elements_count - 1;
    }

    array->size = new_size;
}


void array_push_back(dynamic_array_t *array, void *element)
{
    memcpy(array->buffer + array->end*array->size_of_element,
        element,
        array->size_of_element);

    array->elements_count++;
    array->end = (array->end + 1) & (array->size - 1);

    if (array->elements_count == array->size)
        array_expand_(array);
}


void array_push_front(dynamic_array_t *array, void *element)
{
    memcpy(array->buffer + array->start*array->size_of_element,
        element,
        array->size_of_element);

    array->elements_count++;
    array->start = (array->start - 1) & (array->size - 1);

    if (array->elements_count == array->size)
        array_expand_(array);
}


void array_pop_back(dynamic_array_t *array)
{
    if (array->elements_count == 0)
        return;

    array->end = (array->end - 1) & (array->size - 1);
    array->elements_count--;

    if (array->size > 1 && array->elements_count*4 <= array->size)
        array_shrink_(array);
}


void array_pop_front(dynamic_array_t *array)
{
    if (array->elements_count == 0)
        return;

    array->start = (array->start + 1) & (array->size - 1);
    array->elements_count--;

    if (array->size > 1 && array->elements_count*4 <= array->size)
        array_shrink_(array);
}