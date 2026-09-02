#include "vmm.h"
#include "pmm.h"
#include "paging.h"
#include "ram.h"

page_dict *kernel_page_dict;

// инициализация нормального paging для ядра
void vmm_init(void)
{
    // сейчас размечены только первые 64 мб памяти и память с KERNEL_VIRTUAL_START ведет на них же
    // задача сменить страничный словарь, чтобы оставить только высокий адрес для работы с ядром

    kernel_page_dict = page_dict_create();

    // переброс системных данных наверх
    for (uint32_t phys_addr = 0; phys_addr < (KERNEL_END - KERNEL_VIRTUAL_START); phys_addr += PAGE_SIZE)
    {
        page_dict_map_page_to_phys(kernel_page_dict, KERNEL_VIRTUAL_START + phys_addr, phys_addr, PAGE_KERNEL);
    }

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

    // отображаем все страницы с флагами для MMIO
    uint32_t flags = PAGE_PRESENT | PAGE_RW | PAGE_CACHE_DISABLE | PAGE_WRITETHROUGH;
    for (uint32_t i = 0; i < page_count; i++)
    {
        uint32_t phys_page = phys_aligned + i * PAGE_SIZE;
        uint32_t virt_page = virt_aligned + i * PAGE_SIZE;
        page_dict_map_page_to_phys(kernel_page_dict, virt_page, phys_page, flags);
    }

    // возвращаем указатель, который соответствует данному физическому
    return (void *)(virt_aligned + offset);
}