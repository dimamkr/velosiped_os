#include "paging.h"
#include "pmm.h"
#include "ram.h"

// работает только в ядре при инициализированной таблице ядра покрывающей кучу

// TODO добавить механизм для динамического обновления таблиц ядра
// ВНИМАНИЕ ЕСЛИ В ЯДРЕ СОЗДАСТСЯ НОВАЯ ТАБЛИЦА ТО НИКАКОЙ ПРОЦЕСС НЕ УЗНАЕТ ОБ ЭТОМ
// нужно сделать обработку этого через выделение при прерывании ошибки доступа

// TODO выделять таблицы не в куче ядра а в отдельных фреймах
// это нужно для того, чтобы с помощью счетчика pmm вести учет того, сколько процессов ссылаются на таблицу
// (для этого же эффекта с кучей придется использовать костыли, поэтому решение такое)
// то есть надо сделать отдельный диапазон в памяти занятый процессом ядра, откуда можно отщипывать память под служебные вещи

// в записях первые 11 битов под флаги, оставшиеся под адрес (начала таблицы или начала страницы) выровненный по 4 кб
// в виртуальном адресе первые 10 бит - индекс в директории, 10 бит индекса в таблице, 12 бит - смещение относительно начала страницы

// Макросы для извлечения индексов из виртуального адреса
#define PD_INDEX(virt_addr) ((((uint32_t)virt_addr) >> 22) & (1023))
#define PT_INDEX(virt_addr) ((((uint32_t)virt_addr) >> 12) & (1023))
#define OFFSET(virt_addr) ((uint32_t)virt_addr & 4095)

#define PAGE_ADDR(page) ((uint32_t)page & ~4095) // получить адрес таблицы или страницы
#define PAGE_FLAGS(page) ((uint32_t)page & 4095) // получить флаги записи в директории или таблице

#define CONTAINER_FROM_DIR_ELEMENT(entry) ((page_container_t *)ram_kernel_to_virt((void *)PAGE_ADDR(entry)))

static inline void tlb_cache_flush(uint32_t virt_addr)
{
    asm volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");
}

// общая функция приведения адреса
uint32_t page_dict_vaddr_to_phys(page_dict_t *pd, void *virt_addr)
{
    uint32_t pd_index = PD_INDEX(virt_addr);
    uint32_t pt_index = PT_INDEX(virt_addr);
    uint32_t offset = OFFSET(virt_addr);

    int32_t dir_entry = pd->page_dir->data[pd_index];
    ASSERT(dir_entry & PAGE_PRESENT);

    page_container_t *pt = CONTAINER_FROM_DIR_ELEMENT(dir_entry);
    uint32_t pt_entry = pt->data[pt_index];
    ASSERT(pt_entry & PAGE_PRESENT);

    return PAGE_ADDR(pt_entry) + offset;
}

// возвращает вирт адрес page_container_t
page_container_t *page_container_create(bool clean)
{
    // в будущем переписать без malloc (если понадобится)
    page_container_t *virt = alligned_malloc(PAGE_SIZE, PAGE_SIZE);
    if (clean)
    {
        memset(virt, 0, PAGE_SIZE);
    }
    return virt;
}

void page_container_copy(page_container_t *dst, page_container_t *src)
{
    memcpy(dst->data, src->data, sizeof(page_container_t));
}

void page_container_destroy(page_container_t *this)
{
    free(this);
}

void page_table_set_copied(page_container_t *this)
{
    for (uint32_t j = 0; j < PAGE_TABLE_ENTRIES; j++)
    {
        if (this->data[j] & PAGE_PRESENT)
        {
            pmm_set_alloced(PAGE_ADDR(this->data[j]));
        }
    }
}

void page_table_destroy(page_container_t *this)
{
    for (uint32_t j = 0; j < PAGE_TABLE_ENTRIES; j++)
    {
        if (this->data[j] & PAGE_PRESENT)
        {
            pmm_free_frame(PAGE_ADDR(this->data[j]));
        }
    }
    page_container_destroy(this);
}

void page_table_copy(page_container_t *dst, page_container_t *src)
{
    page_container_copy(dst, src);

    // Увеличиваем счётчики для страниц данных, на которые ссылается таблица
    for (uint32_t j = 0; j < PAGE_TABLE_ENTRIES; j++)
    {
        if (dst->data[j] & PAGE_PRESENT)
        {
            pmm_set_alloced(PAGE_ADDR(dst->data[j]));
        }
    }
}

// Создание нового каталога
page_dict_t *page_dict_create(void)
{
    page_dict_t *pd = malloc(sizeof(page_dict_t));

    pd->page_dir = page_container_create(1);

    return pd;
}

// Вспомогательная: получить (или создать) таблицу для индекса каталога
static page_container_t *get_or_create_table(page_dict_t *pd, uint32_t pd_id, uint32_t flags)
{
    uint32_t entry = pd->page_dir->data[pd_id];
    if (entry & PAGE_PRESENT)
    {
        // Таблица уже существует – возвращаем её виртуальный адрес
        return (page_container_t *)ram_kernel_to_virt((void *)PAGE_ADDR(entry));
    }

    page_container_t *new_table = page_container_create(1);

    pd->page_dir->data[pd_id] = (uint32_t)ram_kernel_to_phys(new_table) | flags | PAGE_PRESENT | PAGE_RW;
    return new_table;
}

// Отобразить виртуальный адрес на любой свободный физический фрейм (адрес кратен 4 кб)
void page_dict_map_page(page_dict_t *pd, uint32_t virt_addr, uint32_t flags)
{
    uint32_t pd_idx = PD_INDEX(virt_addr);
    uint32_t pt_idx = PT_INDEX(virt_addr);

    // Получаем таблицу (создаём, если нужно)
    page_container_t *page_table = get_or_create_table(pd, pd_idx, flags);

    // Выделяем физический фрейм для данных
    uint32_t frame_phys = pmm_alloc_frame();

    // Устанавливаем запись в таблице
    page_table->data[pt_idx] = frame_phys | flags | PAGE_PRESENT | PAGE_RW;

    tlb_cache_flush(virt_addr);
}

void page_dict_map_interval(page_dict_t *pd, uint32_t virt_start, uint32_t size, uint32_t flags)
{
    uint32_t virt_aligned = page_alligned_left(virt_start);
    uint32_t offset = virt_start - virt_aligned;

    // cколько страниц нужно отобразить, чтобы покрыть [virt_start, virt_start+size)
    uint32_t page_count = page_get_num(offset + size);

    for (uint32_t i = 0; i < page_count; i++)
    {
        uint32_t virt_page = virt_aligned + i * PAGE_SIZE;
        // TODO без лишних flush
        page_dict_map_page(pd, virt_page, flags);
    }
}

// Отобразить виртуальный адрес на конкретный физический фрейм (адреса кратны 4 кб)
void page_dict_map_page_to_phys(page_dict_t *pd, uint32_t virt_addr, uint32_t phys_addr, uint32_t flags)
{
    uint32_t pd_idx = PD_INDEX(virt_addr);
    uint32_t pt_idx = PT_INDEX(virt_addr);

    page_container_t *page_table = get_or_create_table(pd, pd_idx, flags);

    pmm_set_alloced(phys_addr);

    page_table->data[pt_idx] = phys_addr | flags | PAGE_PRESENT;

    tlb_cache_flush(virt_addr);
}

// отстатки от деления на 4кб (смещения) одинаковы у вирт и физ адресов
void page_dict_map_interval_to_phys(page_dict_t *pd, uint32_t virt_start, uint32_t phys_start, uint32_t size, uint32_t flags)
{
    uint32_t phys_aligned = page_alligned_left(phys_start);
    uint32_t offset = phys_start - phys_aligned;

    // cколько страниц нужно отобразить, чтобы покрыть [phys, phys+size)
    uint32_t page_count = page_get_num(offset + size);

    // резервируем виртуальный адрес (выровненный по странице)
    uint32_t virt_aligned = page_alligned_left(virt_start);

    if (unlikely(offset != virt_start - virt_aligned))
    {
        PANIC("BAD ADDRS OFFSET");
    }

    for (uint32_t i = 0; i < page_count; i++)
    {
        uint32_t phys_page = phys_aligned + i * PAGE_SIZE;
        uint32_t virt_page = virt_aligned + i * PAGE_SIZE;
        page_dict_map_page_to_phys(pd, virt_page, phys_page, flags);
    }
}

// Убрать отображение страницы
void page_dict_unmap_page(page_dict_t *pd, uint32_t virt_addr)
{
    uint32_t pd_idx = PD_INDEX(virt_addr);
    uint32_t pt_idx = PT_INDEX(virt_addr);

    uint32_t entry = pd->page_dir->data[pd_idx];
    if (!(entry & PAGE_PRESENT))
        PANIC("BAD UNMAP PAGE");

    page_container_t *pt = CONTAINER_FROM_DIR_ELEMENT(entry);
    uint32_t page_entry = pt->data[pt_idx];
    if (!(page_entry & PAGE_PRESENT))
        return; // страница уже не отображена

    // Освобождаем физический фрейм
    uint32_t frame_phys = PAGE_ADDR(page_entry);
    pmm_free_frame(frame_phys);

    // Очищаем запись в таблице
    pt->data[pt_idx] = 0;
    tlb_cache_flush(virt_addr);
}

void page_dict_unmap_interval(page_dict_t *pd, uint32_t virt_start, uint32_t size)
{
    uint32_t virt_aligned = page_alligned_left(virt_start);
    uint32_t offset = virt_start - virt_aligned;

    // cколько страниц нужно отобразить, чтобы покрыть [virt_start, virt_start+size)
    uint32_t page_count = page_get_num(offset + size);

    for (uint32_t i = 0; i < page_count; i++)
    {
        uint32_t virt_page = virt_aligned + i * PAGE_SIZE;
        // TODO без лишних flush
        page_dict_unmap_page(pd, virt_page);
    }
}

// Уничтожить всё адресное пространство
void page_dict_destroy(page_dict_t *pd)
{
    // Освобождаем все страницы в каждой таблице, затем саму таблицу
    for (uint32_t pd_idx = 0; pd_idx < PAGE_DIR_ENTRIES; ++pd_idx)
    {
        uint32_t entry = pd->page_dir->data[pd_idx];
        if (!(entry & PAGE_PRESENT))
            continue;

        page_container_t *pt = CONTAINER_FROM_DIR_ELEMENT(entry);
        page_table_destroy(pt);
    }
    page_container_destroy(pd->page_dir);
    free(pd);
}

//  глубокое копирование
void page_dict_copy(page_dict_t *dst, page_dict_t *src)
{
    for (uint32_t pd_idx = 0; pd_idx < PAGE_DIR_ENTRIES; pd_idx++)
    {
        uint32_t entry = src->page_dir->data[pd_idx];
        if (!(entry & PAGE_PRESENT))
            continue;

        page_container_t *src_table = CONTAINER_FROM_DIR_ELEMENT(entry);

        page_container_t *new_table = page_container_create(0);
        page_table_copy(new_table, src_table);

        dst->page_dir->data[pd_idx] = (uint32_t)ram_kernel_to_phys(new_table) | PAGE_FLAGS(entry);
    }
}

// TODO нельзя использовать так как удаляет чужие таблицы
// void page_dict_copy_linked(page_dict_t *dst, page_dict_t *src)
// {
//     page_container_copy(dst->page_dir, src->page_dir);
//     for (uint32_t pd_idx = 0; pd_idx < PAGE_DIR_ENTRIES; ++pd_idx)
//     {
//         uint32_t entry = src->page_dir->data[pd_idx];
//         if (!(entry & PAGE_PRESENT))
//             continue;
//         page_table_set_copied(CONTAINER_FROM_DIR_ELEMENT(entry));
//     }
// }
