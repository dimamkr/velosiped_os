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