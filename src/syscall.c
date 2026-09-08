#include "syscall.h"
#include "task.h"
#include "konsole.h"
#include "fat32.h"
#include "vmm.h"

// Таблица системных вызовов (номера из Linux)
#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_EXIT 4
#define SYS_BRK 12

typedef int (*syscall_fn)(uint32_t, uint32_t, uint32_t);

// int sys_read(uint32_t file_descriptor, void *buff, uint32_t size);
int sys_write(uint32_t file_descriptor, const char *buff, uint32_t size);
// int sys_open(const char *path, uint32_t flags);
// int sys_close(uint32_t fd);
// void sys_exit(int status);
// int sys_brk(void *addr);

static syscall_fn syscall_table[] = {
    // [SYS_READ] = (syscall_fn)sys_read,
    [SYS_WRITE] = (syscall_fn)sys_write,
    // [SYS_OPEN] = (syscall_fn)sys_open,
    // [SYS_CLOSE] = (syscall_fn)sys_close,
    // [SYS_EXIT] = (syscall_fn)sys_exit,
    // [SYS_BRK] = (syscall_fn)sys_brk,
};

// обработчик системных прерываний
void syscall_handler(isr_data_t *data)
{
    int sysno = data->eax;
    uint32_t arg1 = data->ebx;
    uint32_t arg2 = data->ecx;
    uint32_t arg3 = data->edx;

    if (sysno < sizeof(syscall_table) / sizeof(syscall_fn) && syscall_table[sysno])
    {
        int ret = syscall_table[sysno](arg1, arg2, arg3);
        data->eax = ret; // возвращаемое значение
    }
    else
    {
        data->eax = -1; // неверный номер
    }
}

int sys_write(uint32_t file_descriptor, const char *buff, uint32_t size)
{
    konsole_print(buff);
    return size;
}