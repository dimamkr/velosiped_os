#include "syscall.h"
#include "task.h"
#include "konsole.h"
#include "fat32.h"
#include "vmm.h"
#include "input_manager.h"
#include "string.h"
#include "ram.h"
#include "keyboard.h"

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
int sys_read(uint32_t file_descriptor, void *buff, uint32_t size);
int sys_exit(uint32_t status, uint32_t _, uint32_t __);
// int sys_brk(void *addr);

static syscall_fn syscall_table[] = {
    [SYS_READ] = (syscall_fn)sys_read,
    [SYS_WRITE] = (syscall_fn)sys_write,
    // [SYS_OPEN] = (syscall_fn)sys_open,
    // [SYS_CLOSE] = (syscall_fn)sys_close,
    [SYS_EXIT] = (syscall_fn)sys_exit,
};

// простейшая проверка пользовательского указателя
static bool_t user_ptr_ok(uint32_t ptr, uint32_t size)
{
    if (ptr == 0)
        return false;
    if (ptr >= RAM_VIRTUAL_START)
        return false;
    if (ptr + size < ptr)
        return false;
    if (ptr + size > RAM_VIRTUAL_START)
        return false;
    return true;
}

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

int sys_write(uint32_t fd, const char *buff, uint32_t size)
{
    if (!user_ptr_ok((uint32_t)buff, size))
        return -1;

    for (uint32_t i = 0; i < size; i++)
        konsole_putch(buff[i]);

    return size;
}

int sys_read(uint32_t fd, void *buff, uint32_t size)
{
    // Пока поддерживаем только stdin (0) — это keyboard канал.
    if (fd != 0)
        return -1;

    if (size < sizeof(keyboard_event_t))
        return -1;

    if (!user_ptr_ok((uint32_t)buff, sizeof(keyboard_event_t)))
        return -1;

    // чтение блокирующее процесс
    keyboard_event_t ev;
    while (!task_input_get(INPUT_CHANNEL_KEYBOARD, &ev))
    {
        task_wait_for_input(INPUT_CHANNEL_KEYBOARD);
    }

    memcpy(buff, &ev, sizeof(ev));
    return sizeof(ev);
}

int sys_exit(uint32_t status, uint32_t _, uint32_t __)
{
    (void)status;
    (void)_;
    (void)__;
    task_exit();
    return 0; // недостижимо
}