#include "paging.h"
#include "konsole.h"
#include "heap.h"
#include "system.h"
#include "bitmap.h"

// #pragma GCC optimize("no-optimize-sibling-calls")

extern void enable_paging(void);
extern void paging_load_directory(void *);

// ======== ВКЛЮЧЕНИЕ ПЕЙДЖИНГА (identity mapping) ========

// Каталог страниц и таблица страниц – должны быть выровнены по 4096 байт
static uint32_t page_directory[1024] __attribute__((aligned(4096)));
// static uint32_t page_table[1024] __attribute__((aligned(4096)));

void init_paging(void)
{
    // 1. Включаем PSE (разрешаем 4‑МБ страницы)
    uint32_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 0x00000010; // бит PSE
    asm volatile("mov %0, %%cr4" : : "r"(cr4));

    // 2. Заполняем каталог страниц
    for (int i = 0; i < 1024; i++)
    {
        // Физический адрес = i * 4 МБ (i << 22)
        // Флаги: присутствие (бит 0), чтение/запись (бит 1),
        //        размер страницы 4 МБ (бит 7)
        page_directory[i] = (i << 22) | 0x83;
    }

    // 3. Загружаем адрес каталога в CR3
    paging_load_directory(page_directory);

    // 4. Включаем пейджинг в CR0
    enable_paging();
}