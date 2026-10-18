#include "syscall.h"

void _exit(int status)
{
    _sysenter6(SYS_EXIT, status, 0, 0, 0, 0, 0);

    // сюда нельзя попасть
    for (;;)
        __asm__ volatile("pause");
}
