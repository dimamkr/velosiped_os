// схема физической оперативной памяти

// реальное расположение
#define KERNEL_ENTRY 0x10000
#define VIDEO_MEMORY_ENTRY 0xB8000
#define KERNEL_ENTRY_STACK 0x90000
#define KHEAP_START_BLOCK (4 * MB)
#define KHEAP_END_BLOCK (128 * MB)

// виртуальное
#define KERNEL_VIRTUAL_BASE (3 * GB)