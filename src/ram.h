#ifndef RAM_H
#define RAM_H

// схема физической оперативной памяти

// TODO получать размер ram из прерывания биоса когда все остальное заработает
//  характеристики
#define TOTAL_RAM (256 * MB)

// начало всех системных структур (в виртуальной таблице)
#define KERNEL_VIRTUAL_START 0xC0000000

// системные структуры
#define VIDEO_MEMORY_START (KERNEL_VIRTUAL_START + 0xB8000)
#define KERNEL_ENTRY_START (KERNEL_VIRTUAL_START + 0x10000)
#define KERNEL_ENTRY_STACK (KERNEL_VIRTUAL_START + 0x90000) // временный стек до перехода в задачу ядра
// первые 4 мб под ядро
// границы кучи
#define KHEAP_START (KERNEL_VIRTUAL_START + 4 * MB)
#define KHEAP_END (KERNEL_VIRTUAL_START + 64 * MB)

#define KERNEL_END (KHEAP_END)

// адрес начиная с которого инициализируются адреса для MMIO
#define MMIO_VIRT_BASE 0xF0000000

static inline void *ram_kernel_to_virt(void *phys_addr)
{
    return phys_addr + KERNEL_VIRTUAL_START;
}

static inline void *ram_kernel_to_phys(void *virt_addr)
{
    return virt_addr - KERNEL_VIRTUAL_START;
}

#endif