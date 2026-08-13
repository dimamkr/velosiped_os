#ifndef GDT_INITIALISER
#define GDT_INITIALISER

#include "types.h"

// Describes one GDT entry
typedef struct
{
    uint16_t limit_low;  // the lower 16 bits of the limit
    uint16_t base_low;   // the lower 16 bits of the base
    uint8_t base_middle; // the next 8 bits of the base
    uint8_t access;      // access flags, determines the segment's ring
    uint8_t granularity;
    uint8_t base_high; // the last 8 bits of the base
} __attribute__((packed)) gdt_entry_t;

// Describes a pointer to the GDT
typedef struct
{
    uint16_t limit; // the upper 16 bits of all selector limits
    uint32_t base;  // the address of the first GDT entry
} __attribute__((packed)) gdt_ptr_t;

void gdt_init(void);

#endif
