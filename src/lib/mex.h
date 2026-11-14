#ifndef MEX
#define MEX

#include "types.h"
#include "heap.h"

#define MEX_BLOCK_BIT_SIZE (sizeof(uint32_t) * 8)
#define MEX_BLOCK_SIZE_POWER 5

typedef struct {
    uint32_t *buffer;
    uint32_t size;
    uint8_t size_power;
} mex_t;

mex_t *mex_create();
void mex_destroy(mex_t *mex);

__attribute__((always_inline))
static inline bool_t mex_expand(mex_t *mex)
{
    if (mex->size_power == 25)
        return false;

    uint32_t tree_ind = mex->size / MEX_BLOCK_BIT_SIZE;
    uint32_t i = (mex->size * (MEX_BLOCK_BIT_SIZE - 1) + 1) / MEX_BLOCK_BIT_SIZE;

    mex->size = MEX_BLOCK_BIT_SIZE * mex->size + 1;
    mex->buffer = realloc(mex->buffer, sizeof(uint32_t) * mex->size);

    memset(mex->buffer + (mex->size - 1) / MEX_BLOCK_BIT_SIZE, '\0', sizeof(uint32_t) * ((mex->size - 1) / 2));

    for (;i;i = i / MEX_BLOCK_BIT_SIZE)
    {
        memcpy(mex->buffer + tree_ind + i, mex->buffer + tree_ind, i * sizeof(uint32_t));
        memset(mex->buffer + tree_ind, '\0', i * sizeof(uint32_t));

        tree_ind = (tree_ind - 1) / MEX_BLOCK_BIT_SIZE;
    }

    mex->buffer[0] = 1;

    mex->size_power += MEX_BLOCK_SIZE_POWER;

    return true;
}

__attribute__((always_inline))
static inline uint32_t mex_get_mex(mex_t *mex)
{
    uint32_t tree_ind = 0;
    uint32_t result = 0;

    if (mex->buffer[0] == -1 && !mex_expand(mex))
        return -1;

    for (uint32_t i = mex->size_power;i != -MEX_BLOCK_SIZE_POWER;i -= MEX_BLOCK_SIZE_POWER)
    {
        uint32_t free_index = __builtin_ctz(~mex->buffer[tree_ind]);

        tree_ind = MEX_BLOCK_BIT_SIZE * tree_ind + free_index + 1;
        result |= free_index << i;
    }

    tree_ind = (tree_ind - 1) / MEX_BLOCK_BIT_SIZE;
    uint32_t result_bits = result;

    uint32_t new_value = mex->buffer[tree_ind] | (1 << (result_bits & (MEX_BLOCK_BIT_SIZE - 1)));
    mex->buffer[tree_ind] = new_value;
    result_bits >>= MEX_BLOCK_SIZE_POWER;

    bool_t filled = new_value == -1;
    tree_ind = (tree_ind - 1) / MEX_BLOCK_BIT_SIZE;

    for (;filled & tree_ind != (uint32_t)-1 / MEX_BLOCK_BIT_SIZE;tree_ind = (tree_ind - 1) / MEX_BLOCK_BIT_SIZE)
    {
        new_value = mex->buffer[tree_ind] | (1 << (result_bits & (MEX_BLOCK_BIT_SIZE - 1)));
        mex->buffer[tree_ind] = new_value;
        filled = new_value == -1;
        result_bits >>= MEX_BLOCK_SIZE_POWER;
    }

    return result;
}

__attribute__((always_inline))
static inline void mex_release(mex_t *mex, uint32_t index)
{
    uint32_t tree_ind = 0;

    for (uint32_t i = mex->size_power;i != -MEX_BLOCK_SIZE_POWER;i -= MEX_BLOCK_SIZE_POWER)
    {
        uint32_t next_sym = (index >> i) & (MEX_BLOCK_BIT_SIZE - 1);

        mex->buffer[tree_ind] &= ~(1 << next_sym);

        tree_ind = MEX_BLOCK_BIT_SIZE * tree_ind + next_sym + 1;
    }
}

#endif