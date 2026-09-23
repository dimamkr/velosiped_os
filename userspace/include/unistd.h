#ifndef _USERSPACE_UNISTD_H
#define _USERSPACE_UNISTD_H

// Минимальный Linux-i386-совместимый ABI системных вызовов.

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_EXIT 4
#define SYS_BRK 12

static inline int __syscall3(int n, int a, int b, int c)
{
    int ret;
    asm volatile("int $0x80"
                 : "=a"(ret)
                 : "a"(n), "b"(a), "c"(b), "d"(c)
                 : "memory");
    return ret;
}

static inline int write(int fd, const void *buf, unsigned len)
{
    return __syscall3(SYS_WRITE, fd, (int)buf, (int)len);
}

static inline void exit(int code)
{
    __syscall3(SYS_EXIT, code, 0, 0);
    for (;;)
        asm volatile("pause");
}

#endif