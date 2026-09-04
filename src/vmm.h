// // virtual memory manager
#ifndef VMM_H
#define VMM_H

#include "types.h"
#include "paging.h"

void vmm_init(void);
void *vmm_map_mmio(uint32_t phys, uint32_t size);
page_dict_t *vmm_create_user(uint32_t size);
page_dict_t *vmm_create_process_kernel_page_dict();
void vmm_create_process_memory_paging(page_dict_t *pd, uint32_t virt_start, uint32_t size, uint32_t align);
uint32_t vmm_create_process_stack_paging(page_dict_t *pd, uint32_t size, uint32_t align);

static inline void vmm_page_dict_switch(page_dict_t *prev, page_dict_t *next)
{
    if (prev != next)
    {
        page_dict_switch(next);
    }
}

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