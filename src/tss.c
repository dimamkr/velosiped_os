#include "tss.h"
#include "system.h"

// глобальный tss на ядро
tss_t tss_entry;

void tss_init(uint32_t gdt_date_curr)
{
    // Заполняем TSS
    memset(&tss_entry, 0, sizeof(tss_t));
    tss_entry.ss0 = 0x10;                 // селектор сегмента данных ядра
    tss_entry.iomap_base = sizeof(tss_t); // Закрыть порты ввода-вывода
    tss_entry.esp0 = 0xDEADBEEF;          // стек ядра для каждой задачи загружается потом отдельно

    // Дескриптор TSS (тип 0x89 – Present, 32-bit TSS)
    gdt_set_gate(gdt_date_curr, (uint32_t)&tss_entry, sizeof(tss_t) - 1, 0x89, 0x40);
    // Загружаем TR (task register)
    asm volatile("ltr %%ax" : : "a"(0x28)); // селектор TSS (5-й индекс, RPL=0)
}
