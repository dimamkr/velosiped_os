#include "paging.h"
#include "pmm.h"
#include "ram.h"

// работает только в ядре при инициализированной таблице ядра покрывающей кучу

// TODO переписать нормально через рекурсивное отображение

// в записях первые 11 битов под флаги, оставшиеся под адрес (начала таблицы или начала страницы) выровненный по 4 кб
// в виртуальном адресе первые 10 бит - индекс в директории, 10 бит индекса в таблице, 12 бит - смещение относительно начала страницы

// Макросы для извлечения индексов из виртуального адреса
#define PD_INDEX(virt_addr) ((((uint32_t)virt_addr) >> 22) & (1023))
#define PT_INDEX(virt_addr) ((((uint32_t)virt_addr) >> 12) & (1023))
#define OFFSET(virt_addr) ((uint32_t)virt_addr & 4095)

#define PAGE_ADDR(page) ((uint32_t)page & ~4095) // получить адрес таблицы или страницы
#define PAGE_FLAGS(page) ((uint32_t)page & 4095) // получить флаги записи в директории или таблице

static inline uint32_t to_phys_kernel(void *virt_addr)
{
    return (uint32_t)virt_addr - KERNEL_VIRTUAL_START;
}

static inline void tlb_cache_flush(uint32_t virt_addr)
{
    asm volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");
}

// возвращает вирт адрес page_container
page_container *page_container_create_virt(bool clean)
{
    // переписать без malloc
    page_container *virt = alligned_malloc(PAGE_SIZE, PAGE_SIZE);
    if (clean)
    {
        memset(virt, 0, PAGE_SIZE);
    }
    return virt;
}

void page_container_destroy_virt(page_container *this)
{
    free(this);
}

// Создание нового каталога
page_dict *page_dict_create(void)
{
    page_dict *pd = malloc(sizeof(page_dict));

    pd->page_dir = page_container_create_virt(1);

    pd->page_tables = dynamic_array_create(sizeof(page_container *));

    return pd;
}

// Вспомогательная: получить (или создать) таблицу для индекса каталога
static page_container *get_or_create_table(page_dict *pd, uint32_t pd_id, uint32_t flags)
{
    uint32_t entry = pd->page_dir->data[pd_id];
    if (entry & PAGE_PRESENT)
    {
        // Таблица уже существует – возвращаем её виртуальный адрес
        return (page_container *)PAGE_ADDR(entry);
    }

    page_container *new_table = page_container_create_virt(1);
    dynamic_array_push_back(pd->page_tables, new_table);

    pd->page_dir->data[pd_id] = to_phys_kernel(new_table) | flags | PAGE_PRESENT | PAGE_RW;
    return new_table;
}

// Отобразить виртуальный адрес на любой свободный физический фрейм (адрес кратен 4 кб)
void page_dict_map_page(page_dict *pd, uint32_t virt_addr, uint32_t flags)
{
    uint32_t pd_idx = PD_INDEX(virt_addr);
    uint32_t pt_idx = PT_INDEX(virt_addr);

    // Получаем таблицу (создаём, если нужно)
    page_container *page_table = get_or_create_table(pd, pd_idx, flags);

    // Выделяем физический фрейм для данных
    uint32_t frame_phys = pmm_alloc_frame();

    // Устанавливаем запись в таблице
    page_table->data[pt_idx] = frame_phys | flags | PAGE_PRESENT;

    tlb_cache_flush(virt_addr);
}

// Отобразить виртуальный адрес на конкретный физический фрейм (адреса кратны 4 кб)
void page_dict_map_page_to_phys(page_dict *pd, uint32_t virt_addr, uint32_t phys_addr, uint32_t flags)
{
    uint32_t pd_idx = PD_INDEX(virt_addr);
    uint32_t pt_idx = PT_INDEX(virt_addr);

    page_container *page_table = get_or_create_table(pd, pd_idx, flags);

    pmm_set_alloced_flag(phys_addr);

    page_table->data[pt_idx] = phys_addr | flags | PAGE_PRESENT;

    tlb_cache_flush(virt_addr);
}

// Убрать отображение страницы
void page_dict_unmap_page(page_dict *pd, uint32_t virt_addr)
{
    uint32_t pd_idx = PD_INDEX(virt_addr);
    uint32_t pt_idx = PT_INDEX(virt_addr);

    uint32_t entry = pd->page_dir->data[pd_idx];
    if (!(entry & PAGE_PRESENT))
        PANIC("BAD UNMAP PAGE");

    uint32_t table_phys = entry & ~0xFFF;
    uint32_t *pt = (uint32_t *)table_phys;
    uint32_t page_entry = pt[pt_idx];
    if (!(page_entry & PAGE_PRESENT))
        return; // страница уже не отображена

    // Освобождаем физический фрейм
    uint32_t frame_phys = PAGE_ADDR(page_entry);
    pmm_free_frame(frame_phys);

    // Очищаем запись в таблице
    pt[pt_idx] = 0;
    tlb_cache_flush(virt_addr);
}

// Уничтожить всё адресное пространство
void page_dict_destroy(page_dict *pd)
{
    // Освобождаем все страницы в каждой таблице, затем саму таблицу
    for (uint32_t i = 0; i < pd->page_tables->elements_count; i++)
    {
        page_container *page_table_virt = dynamic_array_get_by_index(pd->page_tables, i);
        for (uint32_t j = 0; j < PAGE_TABLE_ENTRIES; j++)
        {
            if (page_table_virt->data[j] & PAGE_PRESENT)
            {
                pmm_free_frame(PAGE_ADDR(page_table_virt->data[j]));
            }
        }
        page_container_destroy_virt(page_table_virt);
    }
    dynamic_array_destroy(pd->page_tables);
    page_container_destroy_virt(pd->page_dir);
    free(pd);
}

// переключение на данный каталог
void page_dict_switch(page_dict *pd)
{
    paging_load_directory(to_phys_kernel(pd->page_dir));
}