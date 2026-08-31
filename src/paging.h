#ifndef PAGING_H
#define PAGING_H

#include "types.h"
#include "dynamic_array.h"

// ВЕЗДЕ АДРЕС ВИРТУАЛЬНЫЙ ЕСЛИ НЕ УКАЗАНО ОБРАТНОЕ

// Флаги страниц
#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2
#define PAGE_USER 0x4 // пользователь имеет доступ
#define PAGE_WRITETHROUGH 0x8
#define PAGE_CACHE_DISABLE 0x10
#define PAGE_ACCESSED 0x20
#define PAGE_DIRTY 0x40

#define PAGE_KERNEL (PAGE_PRESENT | PAGE_RW) // страница ядра для ядра

#define PAGE_SIZE 4096
#define PAGE_DIR_ENTRIES 1024
#define PAGE_TABLE_ENTRIES 1024

extern void enable_paging(void);
extern void paging_load_directory(uint32_t);

typedef struct
{
    uint32_t data[PAGE_TABLE_ENTRIES] __attribute__((aligned(4096)));
} page_container;

typedef struct
{
    page_container *page_dir;     // вирт адрес директории
    dynamic_array_t *page_tables; // хранит вирт адреса таблиц
} page_dict;

// Создание нового пустого каталога
page_dict *page_dict_create(void);

void page_dict_map_page(page_dict *pd, uint32_t virt_addr, uint32_t flags);
void page_dict_unmap_page(page_dict *pd, uint32_t virt_addr);
void page_dict_destroy(page_dict *pd);
void page_dict_map_page_to_phys(page_dict *pd, uint32_t virt_addr, uint32_t phys_addr, uint32_t flags);
void page_dict_switch(page_dict *pd);

#endif