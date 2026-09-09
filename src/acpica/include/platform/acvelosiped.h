#ifndef __ACVELOSIPED_H__
#define __ACVELOSIPED_H__

#define ACPI_MACHINE_WIDTH 32

#include "types.h"
#include "heap.h"
#include "system.h"
#include "string.h"

typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;

typedef int8_t INT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef int64_t INT64;

typedef uint32_t ACPI_SIZE;
typedef uint32_t ACPI_UINTPTR_T;
typedef uint64_t ACPI_PHYSICAL_ADDRESS;

typedef bool_t BOOLEAN;

typedef void FILE;

#define ACPI_USE_SYSTEM_INTTYPES 0
#define ACPI_USE_SYSTEM_CLIBRARY 0

#ifndef ACPI_DIV_64_BY_32
#define ACPI_DIV_64_BY_32(n_hi, n_lo, d32, q32, r32) \
{                                         \
    asm (                                 \
        "div %2"                          \
        : "+a"(q32), "+d"(r32)            \
        : "r"(d32)                        \
        :                                 \
    );                                    \
}
#endif

#ifndef ACPI_MUL_64_BY_32
#define ACPI_MUL_64_BY_32(n_hi, n_lo, m32, p32, c32) \
{                                         \
    asm (                                 \
        "mul %2"                          \
        : "+a"(p32), "+d"(c32)            \
        : "r"(d32)                        \
        :                                 \
    );                                    \
}
#endif

#ifndef ACPI_SHIFT_LEFT_64_BY_32
#define ACPI_SHIFT_LEFT_64_BY_32(n_hi, n_lo, s32) \
{                                         \
    asm (                                 \
        "and $31, %2\n"                     \
        "shld %2, %0, %1\n"                 \
        "shl %2, %0"                      \
        : "+a"(n_lo), "+d"(n_hi)          \
        : "c"(s32)                        \
        :                                 \
    );                                    \
}
#endif

#ifndef ACPI_SHIFT_RIGHT_64_BY_32
#define ACPI_SHIFT_RIGHT_64_BY_32(n_hi, n_lo, s32) \
{                                         \
    asm (                                 \
        "and $31, %2\n"                     \
        "shrd %2, %1, %0\n"                 \
        "shr %2, %1"                      \
        : "+a"(n_lo), "+d"(n_hi)          \
        :                       "c"(s32)  \
        :                                 \
    );                                    \
}
#endif

#ifndef ACPI_SHIFT_RIGHT_64
#define ACPI_SHIFT_RIGHT_64(n_hi, n_lo) \
{                                         \
    asm (                                 \
        "shr $1, %1\n"                      \
        "rcr $1, %0"                      \
        : "+a"(n_lo), "+d"(n_hi)          \
        :                                 \
        :                                 \
    );                                    \
}
#endif

void *AcpiOsAllocate(ACPI_SIZE Size);
void AcpiOsFree(void *Memory);
void AcpiOsPrintf(const char *Format, ...);
void AcpiOsSleep(UINT64 Milliseconds);

#endif