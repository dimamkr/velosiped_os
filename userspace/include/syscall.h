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

/* ============================================================
 *  Process
 * ============================================================ */

__attribute__((always_inline)) static inline int sys_getpid(void)
{
    return _sysenter6(SYS_GETPID, 0, 0, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_getppid(void)
{
    return _sysenter6(SYS_GETPPID, 0, 0, 0, 0, 0, 0);
}

__attribute__((noreturn)) static inline void sys_exit(int code)
{
    _sysenter6(SYS_EXIT, code, 0, 0, 0, 0, 0);

    /* недостижимо: ядро убивает процесс в SYS_EXIT */
    for (;;)
        __asm__ volatile("pause");
}

__attribute__((always_inline)) static inline int sys_kill(int pid, int sig)
{
    return _sysenter6(SYS_KILL, pid, sig, 0, 0, 0, 0);
}

/* ============================================================
 *  Memory
 * ============================================================ */

/* new_brk == 0 — узнать текущий brk, иначе установить новый.
   Возвращает актуальный brk (или старый, если new_brk вне допустимого диапазона). */
__attribute__((always_inline)) static inline int sys_brk(void *new_brk)
{
    return _sysenter6(SYS_BRK, (int)new_brk, 0, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_mmap2(void *addr, unsigned len,
                                                           int prot, int flags,
                                                           int fd, int offset)
{
    return _sysenter6(SYS_MMAP2, (int)addr, len, prot, flags, fd, offset);
}

__attribute__((always_inline)) static inline int sys_munmap(void *addr, unsigned len)
{
    return _sysenter6(SYS_MUNMAP, (int)addr, len, 0, 0, 0, 0);
}

/* ============================================================
 *  Files
 * ============================================================ */

__attribute__((always_inline)) static inline int sys_open(const char *path, int flags, int mode)
{
    return _sysenter6(SYS_OPEN, (int)path, flags, mode, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_close(int fd)
{
    return _sysenter6(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_lseek(int fd, int offset, int whence)
{
    return _sysenter6(SYS_LSEEK, fd, offset, whence, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_stat(const char *path, void *st)
{
    return _sysenter6(SYS_STAT, (int)path, (int)st, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_fstat(int fd, void *st)
{
    return _sysenter6(SYS_FSTAT, fd, (int)st, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_unlink(const char *path)
{
    return _sysenter6(SYS_UNLINK, (int)path, 0, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_chdir(const char *path)
{
    return _sysenter6(SYS_CHDIR, (int)path, 0, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_getcwd(char *buf, unsigned size)
{
    return _sysenter6(SYS_GETCWD, (int)buf, size, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_dup(int oldfd)
{
    return _sysenter6(SYS_DUP, oldfd, 0, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_access(const char *path, int mode)
{
    return _sysenter6(SYS_ACCESS, (int)path, mode, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_ioctl(int fd, int req, void *arg)
{
    return _sysenter6(SYS_IOCTL, fd, req, (int)arg, 0, 0, 0);
}

/* ============================================================
 *  Time
 * ============================================================ */

/* struct k_timespec { int32_t tv_sec; int32_t tv_nsec; } */
__attribute__((always_inline)) static inline int sys_clock_gettime(int clk, void *ts)
{
    return _sysenter6(SYS_CLOCK_GETTIME, clk, (int)ts, 0, 0, 0, 0);
}

/* struct k_timeval { int32_t tv_sec; int32_t tv_usec; } */
__attribute__((always_inline)) static inline int sys_gettimeofday(void *tv, void *tz)
{
    return _sysenter6(SYS_GETTIMEOFDAY, (int)tv, (int)tz, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_nanosleep(void *req, void *rem)
{
    return _sysenter6(SYS_NANOSLEEP, (int)req, (int)rem, 0, 0, 0, 0);
}

#endif