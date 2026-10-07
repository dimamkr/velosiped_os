// ring.h
#ifndef RING_H
#define RING_H

#include "types.h"

// кольцевая очередь SPSC (Single Producer Single Consumer) на буффере фикс размера
typedef struct
{
    void *buffer;
    uint32_t size; // степень двойки
    uint32_t size_of_element;

    volatile uint32_t start;
    volatile uint32_t end;
} ring_t;

ring_t *ring_create(uint32_t size_of_element, uint32_t size);
void ring_destroy(ring_t *this);

bool_t ring_push_back(ring_t *this, const void *element);
bool_t ring_pop_front(ring_t *this, void *out);
bool_t ring_empty(ring_t *this);
bool_t ring_full(ring_t *this);
uint32_t ring_count(ring_t *this);

#endif