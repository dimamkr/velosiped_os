// physical memory manager
#ifndef PMM_H
#define PMM_H

#include "types.h"

void pmm_init(void);
uint32_t pmm_alloc_frame();
bool pmm_free_frame(uint32_t phys_addr);
void pmm_set_alloced(uint32_t phys_addr);

#endif