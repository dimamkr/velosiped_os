#include "pat.h"
#include "system.h"

#define IA32_PAT_MSR 0x277
#define PAT_UC 0x00 // Uncacheable
#define PAT_WC 0x01 // Write Combining
#define PAT_WT 0x04 // Write Through
#define PAT_WB 0x06 // Write Back

// TODO сверить с осдев вики
void pat_init(void)
{
    uint32_t eax, ebx, ecx, edx;

    // 1. Проверяем поддержку PAT через CPUID (бит 16 в EDX)
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    if (!(edx & (1 << 16)))
    {
        hang_forever();
    }

    // 2. Формируем 64-битное значение для IA32_PAT.
    //    Каждая запись (PA0..PA7) занимает 8 бит. Младшие 3 бита — тип.
    //    Устанавливаем PA4 = WC, остальные — стандартные значения.
    uint64_t pat_value = 0;
    pat_value |= (uint64_t)PAT_WB << (0 * 8); // PA0 = WB
    pat_value |= (uint64_t)PAT_WT << (1 * 8); // PA1 = WT
    pat_value |= (uint64_t)PAT_UC << (2 * 8); // PA2 = UC
    pat_value |= (uint64_t)PAT_UC << (3 * 8); // PA3 = UC
    pat_value |= (uint64_t)PAT_WC << (4 * 8); // PA4 = WC
    pat_value |= (uint64_t)PAT_WT << (5 * 8); // PA5 = WT
    pat_value |= (uint64_t)PAT_UC << (6 * 8); // PA6 = UC
    pat_value |= (uint64_t)PAT_UC << (7 * 8); // PA7 = UC

    // 3. Записываем значение в MSR IA32_PAT.
    __asm__ volatile("wrmsr" : : "c"(IA32_PAT_MSR),
                                 "a"((uint32_t)pat_value),
                                 "d"((uint32_t)(pat_value >> 32)));
}