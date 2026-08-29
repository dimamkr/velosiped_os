#ifndef BITMAP_H
#define BITMAP_H

#include "types.h"

#define BLC 32
#define REM 31

typedef struct
{
    uint32_t *buff;
    uint32_t bits_count;
    uint32_t blocks_count;
} bitmap_t;

static inline void bitmap_set_bit(bitmap_t *this, uint32_t index)
{
    this->buff[index >> 5] |= (1 << (index & REM));
}

static inline void bitmap_clear_bit(bitmap_t *this, uint32_t index)
{
    this->buff[index >> 5] &= ~(1 << (index & REM));
}

static inline bool bitmap_test_bit(bitmap_t *this, uint32_t index)
{
    return this->buff[index >> 5] & (1 << (index & REM));
}

void bitmap_init(bitmap_t *this, uint32_t *buff, uint32_t bits_count);
uint32_t bitmap_find_first_zero(bitmap_t *this, uint32_t start_index);
uint32_t bitmap_find_first_one(bitmap_t *this, uint32_t start_index);
void bitmap_set_interval(bitmap_t *this, uint32_t start_index, uint32_t end_index);
void bitmap_clear_interval(bitmap_t *this, uint32_t start_index, uint32_t end_index);
uint32_t bitmap_alloc_interval(bitmap_t *this, uint32_t length);

#endif