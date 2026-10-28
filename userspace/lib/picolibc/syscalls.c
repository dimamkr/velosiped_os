#include "syscall.h"
#include <errno.h>
#include <stdint.h>

void _exit(int status)
{
    _sysenter6(SYS_EXIT, status, 0, 0, 0, 0, 0);

    // сюда нельзя попасть
    for (;;)
        __asm__ volatile("pause");
}

void *__wrap_sbrk(intptr_t incr)
{
    // узнаем текущий адрес брейка у ядра
    uint32_t current_brk = sys_brk(0);

    // если picolibc просто проверяет кучу
    if (incr == 0)
    {
        return (void *)current_brk;
    }

    // считаем, какой адрес хочет библиотека
    uint32_t next_brk = current_brk + incr;

    // передаем этот адрес в ядро
    uint32_t actual_brk = sys_brk(next_brk);

    // если не получилось
    if (actual_brk != next_brk)
    {
        errno = ENOMEM;
        return (void *)-1;
    }

    // успех: возвращаем адрес до приращения
    return (void *)current_brk;
}