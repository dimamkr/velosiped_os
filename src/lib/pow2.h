#ifndef POW2_H
#define POW2_H

#include "types.h"

static inline int is_pow2(size_t x)
{
    return x != 0 && (x & (x - 1)) == 0;
}

static inline unsigned log2_floor(size_t x) /* x > 0 */
{
    return 31u - (unsigned)__builtin_clz(x);
}

static inline unsigned log2_ceil(size_t x) /* 0 -> 0, 1 -> 0 */
{
    if (x <= 1)
        return 0;
    return 32u - (unsigned)__builtin_clz(x - 1);
}

static inline size_t floor_pow2(size_t x)
{
    return x ? (size_t)1 << log2_floor(x) : 0;
}

static inline size_t ceil_pow2(size_t x)
{
    if (x <= 1)
        return x;
    unsigned e = log2_ceil(x);
    return (e >= 32u) ? 0 : (size_t)1 << e;
}

#endif