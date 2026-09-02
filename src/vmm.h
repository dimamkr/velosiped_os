// // virtual memory manager
#ifndef VMM_H
#define VMM_H

#include "types.h"

void vmm_init(void);
void *vmm_map_mmio(uint32_t phys, uint32_t size);

// мин число страниц содержащих столько-то памяти
static inline uint32_t page_get_num(uint32_t size)
{
    return (size + 4095) >> 12;
}

static uint32_t page_alligned_left(uint32_t addr)
{
    return addr & ~4095;
}

#endif