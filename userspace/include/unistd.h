#ifndef _USERSPACE_UNISTD_H
#define _USERSPACE_UNISTD_H

// Минимальный Linux-i386-совместимый ABI системных вызовов.

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_EXIT 4
#define SYS_BRK 12

#define SYS_WIN_CREATE 200
#define SYS_WIN_COMMIT 201
#define SYS_WIN_MOVE 202
#define SYS_WIN_SET_Z 203
#define SYS_WIN_DESTROY 204
#define SYS_SLEEP 162
#define SYS_POLL 168

// TODO вынести в внешний файл .asm; иначе потенциальные баги
__attribute__((always_inline)) static inline int _sysenter6(int syscall_num, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5)
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
        : "ecx", "edx", "memory"); // нужно предупредить что ebp тут может использоваться через r

    return eax;
}

__attribute__((always_inline)) static inline int _sysenter3(int syscall_num, int arg0, int arg1, int arg2)
{
    return _sysenter6(syscall_num, arg0, arg1, arg2, 0, 0, 0);
}

// TODO вынести в внешний файл .asm; иначе потенциальные баги
__attribute__((always_inline)) static inline int _syscall6_int0x80(int num, int a1, int a2, int a3, int a4, int a5, int a6)
{
    int ret;
    asm volatile(
        "push %%ebp\n"
        "movl %[a6], %%ebp\n"
        "int $0x80\n"
        "pop %%ebp\n"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2), "d"(a3),
          "S"(a4), "D"(a5), [a6] "m"(a6) // a6 копируется в ebp потом
        : "memory");
    return ret;
}

__attribute__((always_inline)) static inline int _syscall3_int0x80(int syscall_num, int arg0, int arg1, int arg2)
{
    return _syscall6_int0x80(syscall_num, arg0, arg1, arg2, 0, 0, 0);
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
    _sysenter6(SYS_EXIT, code, 0, 0, 0, 0, 0);

    while (1)
        asm volatile("pause");
}

__attribute__((always_inline)) static inline int sys_win_create(int w, int h, int x, int y, int z)
{
    return _sysenter6(SYS_WIN_CREATE, w, h, x, y, z, 0);
}

__attribute__((always_inline)) static inline int sys_win_commit(int handle, const void *usr_buf)
{
    return _sysenter6(SYS_WIN_COMMIT, handle, (int)usr_buf, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_win_move(int handle, int x, int y)
{
    return _sysenter6(SYS_WIN_MOVE, handle, x, y, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_win_set_z(int handle, int z)
{
    return _sysenter6(SYS_WIN_SET_Z, handle, z, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_win_destroy(int handle)
{
    return _sysenter6(SYS_WIN_DESTROY, handle, 0, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_sleep(int ms)
{
    return _sysenter6(SYS_SLEEP, ms, 0, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_poll(int fd)
{
    return _sysenter6(SYS_POLL, fd, 0, 0, 0, 0, 0);
}

#endif