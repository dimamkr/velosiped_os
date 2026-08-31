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