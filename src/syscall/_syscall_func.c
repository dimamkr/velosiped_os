#include "_syscall_func.h"
#include "task.h"
#include "konsole.h"
#include "fat32.h"
#include "vmm.h"
#include "input_manager.h"
#include "string.h"
#include "ram.h"
#include "keyboard.h"

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

int sys_write(uint32_t fd, const char *buff, uint32_t size)
{
    if (!user_ptr_ok((uint32_t)buff, size))
        return -1;

    konsole_println("W");
    for (uint32_t i = 0; i < size; i++)
        konsole_putch(buff[i]);

    return size;
}

int sys_read(uint32_t fd, void *buff, uint32_t size)
{
    // Пока поддерживаем только keyboard канал.
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

    konsole_printf(" keycode %d pressed %d\n", ev.keycode, ev.pressed);

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

syscall_fn syscall3_table[SYSCALL3_TABLE_COUNT] = {
    [SYS_READ] = (syscall_fn)sys_read,
    [SYS_WRITE] = (syscall_fn)sys_write,
    // [SYS_OPEN] = (syscall_fn)sys_open,
    // [SYS_CLOSE] = (syscall_fn)sys_close,
    [SYS_EXIT] = (syscall_fn)sys_exit,
};
