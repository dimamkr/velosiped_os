#include "syscall.h"
#include "input.h"

#include <stdio.h>

// TODO переписать нормально

// TODO это тупо заглушка для вывода в консоль
static int __stdout_putc(char c, FILE *file)
{
    (void)file;
    sys_write(1, &c, 1);
    return (unsigned char)c;
}

// TODO это очень тупая заглушка - чтение только ввода с клавиатуры
static int __stdout_getc(FILE *file)
{
    (void)file;
    keyboard_event_t ev;
    while (1)
    {
        int n = sys_read(0, &ev, sizeof(ev));
        if (n != sizeof(ev))
        {
            return -1; // Ошибка чтения
        }
        // Игнорируем отпускание клавиш
        if (!ev.pressed)
        {
            continue;
        }
        // Возвращаем только ASCII-символы
        if (ev.keycode < 0x80)
        {
            return ev.keycode;
        }
        if (ev.keycode == KEY_ENTER)
            return '\n';
    }
}

static FILE __stdio = FDEV_SETUP_STREAM(__stdout_putc, __stdout_getc, NULL, _FDEV_SETUP_RW);

FILE *const stdin = &__stdio;
FILE *const stdout = &__stdio;
FILE *const stderr = &__stdio;