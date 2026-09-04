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

#define ACPI_USE_SYSTEM_INTTYPES    1
#define ACPI_USE_SYSTEM_CLIBRARY    1

void *AcpiOsAllocate(ACPI_SIZE Size);
void AcpiOsFree(void *Memory);
void AcpiOsPrintf(const char *Format, ...);
void AcpiOsSleep(UINT64 Milliseconds);

#endif