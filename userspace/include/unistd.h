#ifndef _USERSPACE_UNISTD_H
#define _USERSPACE_UNISTD_H

// Минимальный Linux-i386-совместимый ABI системных вызовов.

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_EXIT 4
#define SYS_BRK 12

__attribute__((always_inline)) static inline int _sysenter(int syscall_num, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5)
{
    int eax = syscall_num;
    int args[6] = {arg0, arg1, arg2, arg3, arg4, arg5};
    int saved_esp;

    asm volatile(
        "movl %%esp, %1\n\t"    // сохраняем esp в аккумулятор
        "leal %2, %%esp\n\t"    // esp теперь на начале массива аргументов
        "movl %%esp, %%ecx\n\t" // usr esp для sysenter
        "leal 1f, %%edx\n\t"    // адрес возврата для sysenter
        "sysenter\n\t"

        "1:\n\t" // возвращается сюда

        "movl %1, %%esp\n\t" // восстанавливаем esp
        : "+a"(eax), "=r"(saved_esp)
        : "m"(args)
        : "ecx", "edx", "memory");

    return eax;
}

__attribute__((always_inline)) static inline int _sysenter3(int syscall_num, int arg0, int arg1, int arg2)
{
    return _sysenter(syscall_num, arg0, arg1, arg2, 0, 0, 0);
}

__attribute__((always_inline)) static inline int _syscall3_int0x80(int num, int a0, int a1, int a2)
{
    int ret;
    asm volatile("int $0x80"
                 : "=a"(ret)
                 : "a"(num), "b"(a0), "c"(a1), "d"(a2)
                 : "memory");
    return ret;
}

int sys_write(volatile int fd, const volatile void *buf, volatile unsigned len)
{
    return _sysenter3(SYS_WRITE, fd, (int)buf, len);
}

int sys_read(volatile int fd, volatile void *buf, volatile unsigned len)
{
    return _sysenter3(SYS_READ, fd, (int)buf, len);
}

__attribute__((noreturn)) void sys_exit(volatile int code)
{
    _sysenter(SYS_EXIT, code, 0, 0, 0, 0, 0);

    while (1)
        asm volatile("pause");
}

#endif