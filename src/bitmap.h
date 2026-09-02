#ifndef BITMAP_H
#define BITMAP_H

#include "types.h"
#include "pointer_utils.h"

#define BLC 32
#define REM 31

#define BITMAP_BLOCKS_FROM_SIZE(size) ((size) / BLC + !!((size) & REM))

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

static inline bool_t bitmap_test_bit(bitmap_t *this, uint32_t index)
{
    return this->buff[index >> 5] & (1 << (index & REM));
}

bitmap_t *bitmap_create(uint32_t bits_count);
void bitmap_init(bitmap_t *this, uint32_t bits_count);
void bitmap_init_from_buff(bitmap_t *this, void *buff, uint32_t bits_count);
void bitmap_destroy(bitmap_t *this);
uint32_t bitmap_find_first_zero(bitmap_t *this, uint32_t start_index);
uint32_t bitmap_find_first_one(bitmap_t *this, uint32_t start_index);
void bitmap_set_interval(bitmap_t *this, uint32_t start_index, uint32_t end_index);
void bitmap_clear_interval(bitmap_t *this, uint32_t start_index, uint32_t end_index);
uint32_t bitmap_alloc_interval(bitmap_t *this, uint32_t length);

AUTOCLEANUP_DEFINE_FUNC(bitmap)

#endif