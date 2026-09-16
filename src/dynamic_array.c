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

    if (array->size > 1 && array->elements_count * 4 < array->size)
        array_shrink_(array);
}

void dynamic_array_pop_front(dynamic_array_t *array)
{
    if (array->elements_count == 0)
        return;

    array->start = (array->start + 1) & (array->size - 1);
    array->elements_count--;

    if (array->size > 1 && array->elements_count * 4 < array->size)
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

// сортировка Хоара [l_index,r_index]
void dynamic_array_quicksort(dynamic_array_t *array, int32_t l_index, int32_t r_index, dynamic_array_less_cb less)
{
    if (l_index >= r_index)
        return;

    int32_t l = l_index;
    int32_t r = r_index;

    void *mid_value = malloc(array->size_of_element);
    memcpy(mid_value,
           dynamic_array_get_by_index(array, l_index + (rand() % (r_index - l_index + 1))),
           array->size_of_element);

    while (l <= r)
    {
        // Ищем слева элемент >= pivot
        while (less(dynamic_array_get_by_index(array, l), mid_value))
            l++;

        // Ищем справа элемент <= pivot
        while (less(mid_value, dynamic_array_get_by_index(array, r)))
            r--;

        if (l <= r)
        {
            if (l < r)
                memswap(dynamic_array_get_by_index(array, l),
                        dynamic_array_get_by_index(array, r),
                        array->size_of_element);
            l++;
            r--;
        }
    }

    free(mid_value);

    dynamic_array_quicksort(array, l_index, r, less);
    dynamic_array_quicksort(array, l, r_index, less);
}

void dynamic_array_copy(dynamic_array_t *dst, dynamic_array_t *src)
{
    if (dst == src)
        return;

    dst->buffer = realloc(dst->buffer, src->size * src->size_of_element);
    memcpy(dst->buffer, src->buffer, src->size * src->size_of_element);
    dst->size = src->size;
    dst->elements_count = src->elements_count;
    dst->size_of_element = src->size_of_element;
    dst->start = src->start;
    dst->end = src->end;
}

uint32_t dynamic_array_find_first_eq(dynamic_array_t *array, void *value)
{
    for (uint32_t id = 0; id < array->elements_count; ++id)
    {
        if (memcmp(value, dynamic_array_get_by_index(array, id), array->size_of_element))
            return id;
    }

    return array->elements_count;
}