#include "types.h"
#include "system.h"
#include "konsole.h"

void outb(uint16_t port, byte_t value)
{
    asm volatile("outb %1, %0" : : "dN"(port), "a"(value));
}

byte_t inb(uint16_t port)
{
    byte_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

void outw(uint16_t port, uint16_t value)
{
    asm volatile("outw %1, %0" : : "dN"(port), "a"(value));
}

uint16_t inw(uint16_t port)
{
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

void outl(uint16_t port, uint32_t value)
{
    asm volatile("outl %1, %0" : : "dN"(port), "a"(value));
}

uint32_t inl(uint16_t port)
{
    uint32_t ret;
    asm volatile("inl %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

void halt()
{
    asm volatile("hlt");
}

// размер в байтах
__attribute__((optimize("O3,unroll-loops")))
void memcpy(void *dst, const void *src, uint32_t size)
{
    byte_t *_dst = dst;
    const byte_t *_src = src;

    uint32_t i = 0;

    for (; i + 4 <= size; i += 4)
    {
        *((uint32_t *)(_dst + i)) = *((uint32_t *)(_src + i));
    }
    for (; i < size; i++)
    {
        _dst[i] = _src[i];
    }
}

// TODO оптимизировать
//  размер в байтах
void memset(void *ptr, byte_t value, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
    {
        ((byte_t *)ptr)[i] = value;
    }
}

// TODO
// оптимизировать
// размер в байтах; возвращает равны ли
__attribute__((optimize("O3,unroll-loops")))
bool_t memcmp(void *ptr_a, void *ptr_b, uint32_t size)
{
    uint32_t i = 0;

    for (; i+4 <= size; i += 4)
    {
        if (*((uint32_t *)(ptr_a + i)) != *((uint32_t *)(ptr_b + i)))
        {
            return false;
        }
    }

    for (; i < size; i++)
    {
        if (*((byte_t *)(ptr_a + i)) != *((byte_t *)(ptr_b + i)))
        {
            return false;
        }
    }

    return true;
}

__attribute__((optimize("O3,unroll-loops")))
void memswap(void *buff_1, void *buff_2, uint32_t size)
{
    uint32_t i = 0;

    for (;i+4 <= size;i += 4)
    {
        uint32_t tmp = *((uint32_t*)buff_1);
        *((uint32_t*)buff_1) = *((uint32_t*)buff_2);
        *((uint32_t*)buff_2) = tmp;
    }

    for (;i < size;i++)
    {
        byte_t tmp = *((byte_t*)buff_1);
        *((byte_t*)buff_1) = *((byte_t*)buff_2);
        *((byte_t*)buff_2) = tmp;
    }
}

bool_t uint32_less(void *a, void *b)
{
    return *(uint32_t*)a < *(uint32_t*)b;
}

void panic(char *msg, char *file, uint32_t line)
{
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp, esp, eflags;

    interrupt_disable();

    // Читаем общие регистры по отдельности
    asm volatile("mov %%eax, %0" : "=r"(eax));
    asm volatile("mov %%ebx, %0" : "=r"(ebx));
    asm volatile("mov %%ecx, %0" : "=r"(ecx));
    asm volatile("mov %%edx, %0" : "=r"(edx));
    asm volatile("mov %%esi, %0" : "=r"(esi));
    asm volatile("mov %%edi, %0" : "=r"(edi));
    asm volatile("mov %%ebp, %0" : "=r"(ebp));
    asm volatile("mov %%esp, %0" : "=r"(esp));
    asm volatile("pushf; pop %0" : "=r"(eflags));

    // Адрес возврата (EIP)
    uint32_t eip = (uint32_t)__builtin_return_address(0);

    // Читаем сегментные регистры
    uint16_t cs, ds, es, fs, gs, ss;
    asm volatile("mov %%cs, %0" : "=r"(cs));
    asm volatile("mov %%ds, %0" : "=r"(ds));
    asm volatile("mov %%es, %0" : "=r"(es));
    asm volatile("mov %%fs, %0" : "=r"(fs));
    asm volatile("mov %%gs, %0" : "=r"(gs));
    asm volatile("mov %%ss, %0" : "=r"(ss));

    // Вывод с использованием konsole_printf
    konsole_set_panic_color();
    konsole_printf("GURU MEDITATION (%s)\n", msg);
    konsole_set_bad_result_color();
    konsole_printf("File: %s : %d\n", file, line);
    konsole_set_base_color();

    konsole_printf("\n=== REGISTER DUMP ===\n");
    konsole_printf("EAX: %x  EBX: %x\n", eax, ebx);
    konsole_printf("ECX: %x  EDX: %x\n", ecx, edx);
    konsole_printf("ESI: %x  EDI: %x\n", esi, edi);
    konsole_printf("EBP: %x  ESP: %x\n", ebp, esp);
    konsole_printf("EIP: %x  EFLAGS: %x\n", eip, eflags);
    konsole_printf("CS: %x  DS: %x  ES: %x\n", cs, ds, es);
    konsole_printf("FS: %x  GS: %x  SS: %x\n", fs, gs, ss);

    konsole_print("\nSystem halted.\n");
    halt();
}