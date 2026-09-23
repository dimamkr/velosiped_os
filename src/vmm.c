#include "vmm.h"
#include "pmm.h"
#include "paging.h"
#include "ram.h"
#include "task.h"
#include "konsole.h"

page_dict_t *kernel_page_dict;
extern task_t *current_task;

// обработчик page fault
void page_fault_top_handler(isr_data_t registers)
{
    uint32_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));

    konsole_set_bad_result_color();
    konsole_printf("Page fault at 0x%08x from 0x%02x:0x%08x err=0x%x\n",
                   cr2, registers.cs, registers.eip, registers.err_code);
    konsole_printf("  P=%d (0=not present, 1=protection)\n", registers.err_code & 1);
    konsole_printf("  W=%d (0=read,        1=write)\n", (registers.err_code >> 1) & 1);
    konsole_printf("  U=%d (0=kernel,      1=user)\n", (registers.err_code >> 2) & 1);
    konsole_printf("  RSVD=%d\n", (registers.err_code >> 3) & 1);
    konsole_printf("  I=%d (1=instruction fetch)\n", (registers.err_code >> 4) & 1);
    PANIC("PAGE FAULT");
}

// инициализация нормального paging для ядра
void vmm_init(void)
{
    // сейчас размечены только первые 64 мб памяти и память с RAM_VIRTUAL_START ведет на них же
    // задача сменить страничный словарь, чтобы оставить только высокий адрес для работы с ядром

    kernel_page_dict = page_dict_create();

    // бинарник ядра
    page_dict_map_interval_to_phys(kernel_page_dict, KERNEL_ENTRY_START, (KERNEL_ENTRY_START - RAM_VIRTUAL_START), (VIDEO_MEMORY_START - KERNEL_ENTRY_START), PAGE_KERNEL_FLAGS);

    // видеопамять
    page_dict_map_interval_to_phys(kernel_page_dict, VIDEO_MEMORY_START, (VIDEO_MEMORY_START - RAM_VIRTUAL_START),
                                   (VIDEO_MEMORY_END - VIDEO_MEMORY_START), PAGE_KERNEL_FLAGS);

    // куча
    page_dict_map_interval_to_phys(kernel_page_dict, KHEAP_START,
                                   (KHEAP_START - RAM_VIRTUAL_START), (KHEAP_END - KHEAP_START), PAGE_KERNEL_FLAGS);

    // обработчик page fault

    interrupt_register(0x0E, page_fault_top_handler, NULL);

    page_dict_switch(kernel_page_dict);

    // отключаем PSE (этот же бит используется для pat)
    uint32_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 &= ~(1 << 4); // PSE = 0
    asm volatile("mov %0, %%cr4" ::"r"(cr4));

    // глобальный сброс tlb
    asm volatile("mov %%cr3, %%eax\n"
                 "mov %%eax, %%cr3" ::: "eax", "memory");
}

uint32_t vmm_vaddr_to_phys(void *virt_addr)
{
    return page_dict_vaddr_to_phys(current_task->page_dict, virt_addr);
}

static uint32_t mmio_next_virt = MMIO_VIRT_BASE;

static inline void *vmm_map_special(uint32_t phys, uint32_t size, uint32_t flags)
{
    uint32_t phys_aligned = page_alligned_left(phys);
    uint32_t offset = phys - phys_aligned;

    // cколько страниц нужно отобразить, чтобы покрыть [phys, phys+size)
    uint32_t page_count = page_get_num(offset + size);

    // резервируем виртуальный адрес (выровненный по странице)
    uint32_t virt_aligned = mmio_next_virt;
    mmio_next_virt += page_count * PAGE_SIZE;

    page_dict_map_interval_to_phys(kernel_page_dict, virt_aligned, phys_aligned, page_count * PAGE_SIZE,
                                   flags);

    // возвращаем указатель, который соответствует данному физическому
    return (void *)(virt_aligned + offset);
}

// вирт аллокация целого числа страниц начиная с физ адреса
// возвращает виртуальный адрес данного физического
// (если страница уже была выделена под mmio то все будет в порядке, если выровненный физ адрес тот же)
void *vmm_map_mmio(uint32_t phys, uint32_t size)
{
    return vmm_map_special(phys, size, PAGE_PRESENT | PAGE_RW | PAGE_CACHE_DISABLE | PAGE_WRITETHROUGH);
}

void *vmm_map_framebuffer(uint32_t phys, uint32_t size)
{
    return vmm_map_special(phys, size, PAGE_PRESENT | PAGE_RW | PAGE_WRITE_COMBINE);
}

void vmm_unmap_page(void *virt_addr)
{
    page_dict_unmap_page(current_task->page_dict, (uint32_t)virt_addr);
}

// для создания процессов
//-------------------------------------------------------------------

static inline void copy_linked_kernel_page_dict(page_dict_t *pd)
{
    // TODO пока copy_linked нельзя
    // page_dict_copy_linked(pd, kernel_page_dict);

    page_dict_copy(pd, kernel_page_dict);
}

page_dict_t *vmm_create_process_kernel_page_dict()
{
    page_dict_t *pd = page_dict_create();
    copy_linked_kernel_page_dict(pd);

    return pd;
}

// возвращает верхушку
uint32_t vmm_user_stack_create(page_dict_t *page_dict, uint32_t user_stack_size)
{
    uint32_t user_stack_start = USER_STACK_TOP - user_stack_size;
    page_dict_map_interval(page_dict, user_stack_start, user_stack_size, PAGE_USER_FLAGS);

    return USER_STACK_TOP;
}