#include "konsole.h"
#include "dynamic_array.h"
#include "task.h"
#include "ram.h"
#include <stdarg.h>

// адрес начала видеопамяти
volatile konsole_symbol_t *konsole_start = (konsole_symbol_t *)VIDEO_MEMORY_ENTRY;

int konsole_curr_x;
int konsole_curr_y;
byte_t konsole_current_color;
dynamic_array_t *konsole_output_history;
// индекс верхнего левого символа
uint32_t konsole_output_history_start_index;

void konsole_init()
{
    konsole_curr_x = 0;
    konsole_curr_y = 0;
    konsole_set_base_color();

    konsole_output_history = dynamic_array_create(sizeof(konsole_symbol_t));
    for (uint32_t i = 0; i < KONSOLE_W * KONSOLE_H; ++i)
    {
        dynamic_array_push_back(konsole_output_history, konsole_start + i);
    }

    konsole_output_history_start_index = 0;
}

// возвращает получилось ли проскроллить чтобы не выйти за границы массива
bool konsole_view_scroll_up()
{
    if (konsole_output_history_start_index >= KONSOLE_W)
    {
        konsole_output_history_start_index -= KONSOLE_W;
        memcpy(konsole_start, dynamic_array_get_by_index(konsole_output_history, konsole_output_history_start_index),
               sizeof(konsole_symbol_t) * KONSOLE_W * KONSOLE_H);
        return true;
    }
    return false;
}

bool konsole_view_scroll_down()
{
    if (konsole_output_history_start_index + KONSOLE_H * KONSOLE_W + KONSOLE_W <= konsole_output_history->elements_count)
    {
        konsole_output_history_start_index += KONSOLE_W;
        memcpy(konsole_start, dynamic_array_get_by_index(konsole_output_history, konsole_output_history_start_index),
               sizeof(konsole_symbol_t) * KONSOLE_W * KONSOLE_H);
        return true;
    }
    return false;
}

// static inline konsole_symbol_t *konsole_pos_get()
// {
//     return konsole_start + (konsole_curr_y * KONSOLE_W) + konsole_curr_x;
// }

static inline void konsole_set_value(uint32_t x, uint32_t y, konsole_symbol_t value)
{
    uint32_t index = (y * KONSOLE_W) + x;
    konsole_start[index] = value;
    dynamic_array_set_by_index(konsole_output_history, konsole_output_history_start_index + index, &value);
}

void konsole_cursor_set_position(uint16_t position)
{
    // Установить младший байт (регистр 0x0F)
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(position & 0xFF));

    // Установить старший байт (регистр 0x0E)
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((position >> 8) & 0xFF));
}

// сдвижка позиции
void konsole_pos_shift(int delta_x)
{
    konsole_curr_y += delta_x / KONSOLE_W;
    konsole_curr_x += delta_x % KONSOLE_W;
    konsole_curr_y += konsole_curr_x / KONSOLE_W;
    konsole_curr_x %= KONSOLE_W;

    if (konsole_curr_y >= KONSOLE_H)
    {
        konsole_scroll_down();
    }

    konsole_cursor_set_position(konsole_curr_y * KONSOLE_W + konsole_curr_x);
}

void konsole_clear()
{
    for (int y = 0; y < KONSOLE_H; y++)
    {
        for (int x = 0; x < KONSOLE_W; x++)
        {
            int offset = (y * KONSOLE_W) + x;
            konsole_start[offset].symbol = ' ';
            konsole_start[offset].colors = konsole_current_color;

            konsole_set_value(x, y, (konsole_symbol_t){.symbol = ' ', .colors = konsole_current_color});
        }
    }
    konsole_curr_x = 0;
    konsole_curr_y = 0;
}

void konsole_putch(char ch)
{
    // чтобы curr_x и curr_y были в области видимости
    while (konsole_view_scroll_down())
    {
    }

    switch (ch)
    {
    case '\n':
        konsole_curr_x = 0;
        konsole_curr_y++;

        if (konsole_curr_y >= KONSOLE_H)
        {
            konsole_scroll_down();
        }
        break;

    case '\b':
        if (konsole_curr_x > 0)
        {
            konsole_pos_shift(-1);
            konsole_set_value(konsole_curr_x, konsole_curr_y, (konsole_symbol_t){.symbol = 0, .colors = 0});
        }
        break;

    default:
        konsole_set_value(konsole_curr_x, konsole_curr_y, (konsole_symbol_t){.symbol = ch, .colors = konsole_current_color});
        konsole_pos_shift(1);
    }
}

void konsole_print(const char *text)
{
    task_lock();
    for (int i = 0; text[i] != 0; ++i)
    {
        konsole_putch(text[i]);
    }
    task_unlock();
}

void konsole_println(const char *text)
{
    task_lock();
    konsole_print(text);
    konsole_print("\n");
    task_unlock();
}

void konsole_printf(const char *format, ...)
{
    task_lock();
    va_list args;
    va_start(args, format);

    while (*format)
    {
        if (*format == '%')
        {
            format++;
            switch (*format)
            {
            case 's':
            {
                const char *str = va_arg(args, const char *);
                while (*str)
                {
                    konsole_putch(*str++);
                }
                break;
            }
            case 'd':
            {
                int num = va_arg(args, int);
                if (num < 0)
                {
                    konsole_putch('-');
                    num = -num;
                }
                char tmp[12];
                int len = 0;
                do
                {
                    tmp[len++] = '0' + (num % 10);
                    num /= 10;
                } while (num > 0);
                while (len > 0)
                {
                    konsole_putch(tmp[--len]);
                }
                break;
            }
            case 'x':
            {
                uint32_t num = va_arg(args, uint32_t);
                char tmp[12];
                int len = 0;
                do
                {
                    int digit = num % 16;
                    if (digit < 10)
                        tmp[len++] = '0' + digit;
                    else
                        tmp[len++] = 'a' + digit - 10;
                    num /= 16;
                } while (num > 0);
                while (len > 0)
                {
                    konsole_putch(tmp[--len]);
                }
                break;
            }
            case '%':
            {
                konsole_putch('%');
                break;
            }
            default:
            {
                konsole_putch('%');
                konsole_putch(*format);
                break;
            }
            }
        }
        else
        {
            konsole_putch(*format);
        }
        format++;
    }
    va_end(args);
    task_unlock();
}

// TODO блокировка прерываний тут
void konsole_scroll_down()
{
    memcpy(konsole_start, konsole_start + KONSOLE_W, ((KONSOLE_H - 1) * KONSOLE_W) * sizeof(konsole_symbol_t));

    konsole_output_history_start_index += KONSOLE_W;

    // Очищаем последнюю строку
    for (int x = 0; x < KONSOLE_W; x++)
    {
        konsole_symbol_t value = (konsole_symbol_t){.symbol = ' ', .colors = konsole_current_color};

        dynamic_array_push_back(konsole_output_history, &value);
        konsole_set_value(x, KONSOLE_H - 1, value);
    }

    konsole_curr_y = KONSOLE_H - 1;
}

// COLORS
void konsole_set_color(uint32_t fg, uint32_t bg)
{
    konsole_current_color = (bg << 4) | (fg & 0x0F);
}

void konsole_set_preambula_color()
{
    konsole_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
}

void konsole_set_good_result_color()
{
    konsole_set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
}

void konsole_set_warning_color()
{
    konsole_set_color(COLOR_YELLOW, COLOR_BLACK);
}

void konsole_set_bad_result_color()
{
    konsole_set_color(COLOR_RED, COLOR_BLACK);
}

void konsole_set_panic_color()
{
    konsole_set_color(COLOR_LIGHT_RED, COLOR_BLACK);
}

void konsole_set_base_color()
{
    konsole_set_color(COLOR_WHITE, COLOR_BLACK);
}
