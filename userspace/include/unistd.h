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

    asm volatile(
        "push %6\n"
        "push %5\n"
        "push %4\n"
        "push %3\n"
        "push %2\n"
        "push %1\n"
        "movl %%esp, %%ecx\n"
        "leal 1f, %%edx\n"
        "sysenter\n"
        "1:\n"
        "add $24, %%esp"
        : "+a"(eax)
        : "g"(arg0), "g"(arg1), "g"(arg2), "g"(arg3), "g"(arg4), "g"(arg5)
        : "ecx", "edx", "memory");

    return eax;
}

__attribute__((always_inline)) static inline int _syscall_int0x80(int num, int a0, int a1, int a2)
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
    return _syscall_int0x80(SYS_WRITE, fd, (int)buf, len, 0, 0, 0);
}

int sys_read(volatile int fd, volatile void *buf, volatile unsigned len)
{
    return _syscall_int0x80(SYS_READ, fd, (int)buf, len);
}

__attribute__((noreturn)) void sys_exit(volatile int code)
{
    _sysenter(SYS_EXIT, code, 0, 0, 0, 0, 0);

    while (1)
        asm volatile("pause");
}

#endif