#ifndef SSE_H
#define SSE_H

#include "types.h"

#define FPU_CONTEXT_ALING 16

typedef struct
{
    uint8_t data[512];
} __attribute__((aligned(FPU_CONTEXT_ALING))) fpu_context_t;

fpu_context_t *fpu_context_create(void);

static inline void fpu_context_save(fpu_context_t *fctx)
{
    asm volatile("fxsave %[fctx]\n" : [fctx] "=m"(*fctx)::"memory");
}

static inline void fpu_context_restore(fpu_context_t *fctx)
{
    asm volatile("fxrstor %[fctx]\n" ::[fctx] "m"(*fctx) : "memory");
}

bool_t sse_init(void);

#endif