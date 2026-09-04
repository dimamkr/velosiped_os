#include "acpiosxf.h"
#include "heap.h"
#include "ram.h"


void *AcpiOsAllocate(ACPI_SIZE Size)
{
    return malloc(Size);
}

void AcpiOsFree(void *Memory)
{
    free(Memory);
}

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length)
{
    // У тебя включено identity mapping? Тогда просто верни адрес как есть.
    return (void *)Where;
}

void AcpiOsUnmapMemory(void *LogicalAddress, ACPI_SIZE Size) {
    // Если не используешь MapMemory, просто оставь пустым.
}