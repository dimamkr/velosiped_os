#include "random.h"

static uint32_t seed = 0;

void srand(uint32_t s)
{
    seed = s;
}

uint32_t rand()
{
    // Параметры из ANSI C (period = 2^31)
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    return seed;
}