#ifndef _USERSPACE_UNISTD_H
#define _USERSPACE_UNISTD_H

// Linux i386 ABI (совместимы с Linux)
#define SYS_EXIT 1
#define SYS_READ 3
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6
#define SYS_UNLINK 10
#define SYS_CHDIR 12
#define SYS_LSEEK 19
#define SYS_GETPID 20
#define SYS_ACCESS 33
#define SYS_KILL 37
#define SYS_MKDIR 39
#define SYS_RMDIR 40
#define SYS_DUP 41
#define SYS_BRK 45
#define SYS_IOCTL 54
#define SYS_GETPPID 64
#define SYS_GETTIMEOFDAY 78
#define SYS_MUNMAP 91
#define SYS_STAT 106
#define SYS_FSTAT 108
#define SYS_NANOSLEEP 162
#define SYS_GETCWD 183
#define SYS_MMAP2 192
#define SYS_CLOCK_GETTIME 265

// не совместимо с Linux (доступны номера 500-511)
#define SYS_WIN_CREATE 500
#define SYS_WIN_COMMIT 501
#define SYS_WIN_MOVE 502
#define SYS_WIN_SET_Z 503
#define SYS_WIN_DESTROY 504
#define SYS_SLEEP 505
#define SYS_POLL 506

extern int _sysenter6(int syscall_num, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5);

__attribute__((always_inline)) static inline int _sysenter3(int syscall_num, int arg0, int arg1, int arg2)
{
    return _sysenter6(syscall_num, arg0, arg1, arg2, 0, 0, 0);
}

__attribute__((always_inline)) static inline int _syscall6_int0x80(int num, int a1, int a2, int a3, int a4, int a5, int a6)
{
    int eax = num;

    __asm__ volatile(
        "pushl %[a6]\n\t"          // копируем в стек пока адресация еще верна
        "pushl %%ebp\n\t"          // сохраняем ebp
        "movl 4(%%esp), %%ebp\n\t" // забираем a6 из стека
        "int $0x80\n\t"
        "popl %%ebp\n\t" // восстанавливаем ebp
        "addl $4, %%esp" // восстанавливаем esp
        : "+a"(eax)
        : "b"(a1),
          "c"(a2),
          "d"(a3),
          "S"(a4),
          "D"(a5),
          [a6] "m"(a6)
        : "memory", "cc"); // cc - изменяет флаги процессора

    return eax;
}

static inline int sys_write(volatile int fd, const volatile void *buf, volatile unsigned len)
{
    return _sysenter3(SYS_WRITE, fd, (int)buf, len);
}

static inline int sys_read(volatile int fd, volatile void *buf, volatile unsigned len)
{
    return _sysenter3(SYS_READ, fd, (int)buf, len);
}

// __attribute__((noreturn)) void sys_exit(volatile int code)
// {
//     _sysenter6(SYS_EXIT, code, 0, 0, 0, 0, 0);

//     while (1)
//         asm volatile("pause");
// }

__attribute__((always_inline)) static inline int sys_sleep(int ms)
{
    return _sysenter6(SYS_SLEEP, ms, 0, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_poll(int fd)
{
    return _sysenter6(SYS_POLL, fd, 0, 0, 0, 0, 0);
}

#endif