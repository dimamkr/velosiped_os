#include "ring.h"
#include "heap.h"

// размер обязательно степень двойки
ring_t *ring_create(uint32_t size_of_element, uint32_t size)
{
    if (size <= 0 || (size & (size - 1)))
        return NULL;

    ring_t *r = malloc(sizeof(ring_t));
    if (!r)
        return NULL;
    r->size_of_element = size_of_element;
    r->buffer = malloc(size * size_of_element);
    if (!r->buffer)
    {
        free(r);
        return NULL;
    }
    r->size = size;

    r->start = 0;
    r->end = 0;

    return r;
}

void ring_destroy(ring_t *this)
{
    free(this->buffer);
    free(this);
}

static inline uint32_t _ring_next(ring_t *this, uint32_t idx)
{
    return (idx + 1) & (this->size - 1);
}

bool_t ring_push_back(ring_t *this, const void *element)
{
    uint32_t next = _ring_next(this, this->end);
    if (next == this->start)
        return false; // полон

    memcpy(this->buffer + this->end * this->size_of_element,
           element,
           this->size_of_element);
    this->end = next;
    return true;
}

bool_t ring_pop_front(ring_t *this, void *out)
{
    if (this->start == this->end)
        return false; // пуст

    if (out)
        memcpy(out, this->buffer + this->start * this->size_of_element,
               this->size_of_element);
    this->start = _ring_next(this, this->start);
    return true;
}

uint32_t ring_count(ring_t *this)
{
    return (this->end - this->start) & (this->size - 1);
}

bool_t ring_full(ring_t *this)
{
    return _ring_next(this, this->end) == this->start;
}

bool_t ring_empty(ring_t *this)
{
    return this->start == this->end;
}