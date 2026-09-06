#include "vmm.h"
#include "pmm.h"
#include "paging.h"
#include "ram.h"
#include "task.h"

page_dict_t *kernel_page_dict;
extern task_t *current_task;

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

    page_dict_switch(kernel_page_dict);
}

static uint32_t mmio_next_virt = MMIO_VIRT_BASE;
// вирт аллокация целого числа страниц начиная с физ адреса
// возвращает виртуальный адрес данного физического
// (если страница уже была выделена под mmio то все будет в порядке, если выровненный физ адрес тот же)
void *vmm_map_mmio(uint32_t phys, uint32_t size)
{
    uint32_t phys_aligned = page_alligned_left(phys);
    uint32_t offset = phys - phys_aligned;

    // cколько страниц нужно отобразить, чтобы покрыть [phys, phys+size)
    uint32_t page_count = page_get_num(offset + size);

    // резервируем виртуальный адрес (выровненный по странице)
    uint32_t virt_aligned = mmio_next_virt;
    mmio_next_virt += page_count * PAGE_SIZE;

    page_dict_map_interval_to_phys(kernel_page_dict, virt_aligned, phys_aligned, page_count * PAGE_SIZE,
                                   PAGE_PRESENT | PAGE_RW | PAGE_CACHE_DISABLE | PAGE_WRITETHROUGH);

    // возвращаем указатель, который соответствует данному физическому
    return (void *)(virt_aligned + offset);
}

void vmm_unmap_page(uint32_t virt_addr)
{
    page_dict_unmap_page(current_task->page_dict, virt_addr);
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