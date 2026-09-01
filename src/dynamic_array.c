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

void dynamic_array_push_back(dynamic_array_t *array, void *element)
{
    memcpy(array->buffer + array->end * array->size_of_element,
           element,
           array->size_of_element);

    array->elements_count++;
    array->end = (array->end + 1) & (array->size - 1);

    if (array->elements_count == array->size)
        array_expand_(array);
}

void dynamic_array_push_front(dynamic_array_t *array, void *element)
{
    memcpy(array->buffer + array->start * array->size_of_element,
           element,
           array->size_of_element);

    array->elements_count++;
    array->start = (array->start - 1) & (array->size - 1);

    if (array->elements_count == array->size)
        array_expand_(array);
}

void dynamic_array_pop_back(dynamic_array_t *array)
{
    if (array->elements_count == 0)
        return;

    array->end = (array->end - 1) & (array->size - 1);
    array->elements_count--;

    if (array->size > 1 && array->elements_count * 4 <= array->size)
        array_shrink_(array);
}

void dynamic_array_pop_front(dynamic_array_t *array)
{
    if (array->elements_count == 0)
        return;

    array->start = (array->start + 1) & (array->size - 1);
    array->elements_count--;

    if (array->size > 1 && array->elements_count * 4 <= array->size)
        array_shrink_(array);
}

void dynamic_array_clear(dynamic_array_t *array)
{
    if (array->elements_count == 0)
        return;

    array->start = 0;
    array->end = 0;
    array->elements_count = 0;
    array->size = 1;
    array->buffer = realloc(array->buffer, array->size_of_element);
}

void dynamic_array_quicksort(dynamic_array_t *array, uint32_t l_index, uint32_t r_index, dynamic_array_less_cb less)
{
    if (l_index == r_index || r_index == -1)
        return;

    uint32_t l = l_index;
    uint32_t r = r_index;
    void *l_value = dynamic_array_get_by_index(array, l);
    void *r_value = dynamic_array_get_by_index(array, r);
    void *mid_value = malloc(array->size_of_element);
    
    memcpy(mid_value,
    dynamic_array_get_by_index(array, l_index + (rand() % (r_index - l_index + 1))),
    array->size_of_element);

    while (l <= r)
    {
        bool_t cmp_l_mid = less(l_value, mid_value);
        bool_t cmp_r_mid = less(mid_value, r_value);

        if (cmp_l_mid)
        {
            l++;
            l_value = dynamic_array_get_by_index(array, l);
        }
        else if (cmp_r_mid)
        {
            if (r > 0) r--;
            r_value = dynamic_array_get_by_index(array, r);
        }
        else if (!cmp_l_mid && !cmp_r_mid)
        {
            memswap(l_value, r_value, array->size_of_element);
            l++;
            if (r > 0) r--;
            l_value = dynamic_array_get_by_index(array, l);
            r_value = dynamic_array_get_by_index(array, r);
        }
    }

    if (l != l_index)
        dynamic_array_quicksort(array, l_index, l - 1, less);
    if (l < r_index)
        dynamic_array_quicksort(array, l, r_index, less);
}