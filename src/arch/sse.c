#include "sse.h"
#include "heap.h"
#include "system.h"

static fpu_context_t fpu_template;

// поддерживается ли SSE, FXSR
static inline bool_t _check_for_sse(void)
{
    uint32_t eax, ebx, ecx, edx;

    asm volatile(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1));

    const uint32_t mask = (1 << 24) | (1 << 25); // бит 24 (FXSR) и бит 25 (SSE)
    return (edx & mask) == mask;
}

static inline void _enable_sse(void)
{
    asm volatile(
        ".intel_syntax noprefix\n"

        "mov eax, cr0\n"
        "and ax, 0xFFFB\n" // clear coprocessor emulation CR0.EM
        "or ax, 0x2\n"     // set coprocessor monitoring  CR0.MP
        "mov cr0, eax\n"
        "mov eax, cr4\n"
        "or ax, 3 << 9\n" // set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
        "mov cr4, eax\n"

        ".att_syntax\n"
        :
        :
        : "eax", "cc" // "cc" нужен, так как and/or меняют флаги процессора
    );
    return;
}

static inline void _create_fpu_template(void)
{
    asm volatile("fninit"); // сброс x87
    // установка MXCSR для SSE (все исключения замаскированы)
    uint32_t mxcsr = 0x1F80;
    asm volatile("ldmxcsr %0" ::"m"(mxcsr));
    // сохранение полученного "чистого" состояния в шаблон
    asm volatile("fxsave %0" ::"m"(fpu_template));
}

bool_t sse_init(void)
{
    if (!_check_for_sse())
        return false;

    _enable_sse();

    _create_fpu_template();

    return true;
}

fpu_context_t *fpu_context_create(void)
{
    fpu_context_t *r = (fpu_context_t *)alligned_malloc(sizeof(fpu_context_t), FPU_CONTEXT_ALING);
    memcpy(r, &fpu_template, sizeof(fpu_context_t));
    return r;
}