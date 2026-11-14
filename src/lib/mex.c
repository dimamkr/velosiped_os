#include "mex.h"


mex_t *mex_create()
{
    mex_t *result = malloc(sizeof(mex_t));

    result->buffer = malloc(sizeof(uint32_t));
    memset(result->buffer, '\0', sizeof(uint32_t));
    result->size_power = 0;
    result->size = 1;

    return result;
}

void mex_destroy(mex_t *mex)
{
    free(mex->buffer);
    free(mex);
}