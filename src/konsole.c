#include "konsole.h"
#include "system.h"

#include <stdarg.h>

volatile char *const konsole_start = (char *)0xB8000;
int const konsole_w = 80;
int const konsole_h = 25;
int const konsole_line_offset = konsole_w * 2;

int konsole_curr_x = 0;
int konsole_curr_y = 0;
uint32_t konsole_current_color = 0x0F; // Белый на черном

static char *konsole_pos_get()
{
    return konsole_start + (konsole_curr_y * konsole_line_offset) + (konsole_curr_x * 2);
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
    konsole_curr_y += delta_x / konsole_w;
    konsole_curr_x += delta_x % konsole_w;
    konsole_curr_y += konsole_curr_x / konsole_w;
    konsole_curr_x %= konsole_w;

    if (konsole_curr_y >= konsole_h)
    {
        konsole_scroll();
    }

    konsole_cursor_set_position(konsole_curr_y * konsole_w + konsole_curr_x);
}

void konsole_clear()
{
    for (int y = 0; y < konsole_h; y++)
    {
        for (int x = 0; x < konsole_w; x++)
        {
            int offset = (y * konsole_line_offset) + (x * 2);
            konsole_start[offset] = ' ';
            konsole_start[offset + 1] = konsole_current_color;
        }
    }
    konsole_curr_x = 0;
    konsole_curr_y = 0;
}

void konsole_putch(char ch)
{
    switch (ch)
    {
    case '\n':
        konsole_curr_x = 0;
        konsole_curr_y++;
        if (konsole_curr_y >= konsole_h)
        {
            konsole_scroll();
        }
        break;

    case '\b':
        if (konsole_curr_x > 0)
        {
            konsole_pos_shift(-1);
            konsole_pos_get()[0] = 0;
        }
        break;

    default:
        konsole_pos_get()[0] = ch;
        konsole_pos_get()[1] = konsole_current_color;
        konsole_pos_shift(1);
    }
}

void konsole_print(const char *text)
{
    for (int i = 0; text[i] != 0; ++i)
    {
        konsole_putch(text[i]);
    }
}

void konsole_println(const char *text)
{
    konsole_print(text);
    konsole_print("\n");
}

void konsole_printf(const char *format, ...)
{
    char buffer[1024]; // достаточно для большинства сообщений
    int pos = 0;
    va_list args;
    va_start(args, format);

    while (*format && pos < sizeof(buffer) - 1)
    {
        if (*format == '%')
        {
            format++;
            switch (*format)
            {
            case 's':
            {
                const char *str = va_arg(args, const char *);
                while (*str && pos < sizeof(buffer) - 1)
                {
                    buffer[pos++] = *str++;
                }
                break;
            }
            case 'd':
            {
                int num = va_arg(args, int);
                if (num < 0)
                {
                    buffer[pos++] = '-';
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
                    buffer[pos++] = tmp[--len];
                }
                break;
            }
            case 'x':
            {
                uint32_t num = va_arg(args, uint32_t);
                buffer[pos++] = '0';
                buffer[pos++] = 'x';
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
                    buffer[pos++] = tmp[--len];
                }
                break;
            }
            case '%':
            {
                buffer[pos++] = '%';
                break;
            }
            default:
            {
                buffer[pos++] = '%';
                buffer[pos++] = *format;
                break;
            }
            }
        }
        else
        {
            buffer[pos++] = *format;
        }
        format++;
    }
    buffer[pos] = '\0';
    konsole_print(buffer);
    va_end(args);
}

void konsole_set_color(uint32_t fg, uint32_t bg)
{
    konsole_current_color = (bg << 4) | (fg & 0x0F);
}

void konsole_scroll()
{
    // Простая реализация скроллинга
    for (int y = 0; y < konsole_h - 1; y++)
    {
        for (int x = 0; x < konsole_w; x++)
        {
            int src_offset = ((y + 1) * konsole_line_offset) + (x * 2);
            int dst_offset = (y * konsole_line_offset) + (x * 2);
            konsole_start[dst_offset] = konsole_start[src_offset];
            konsole_start[dst_offset + 1] = konsole_start[src_offset + 1];
        }
    }
    // Очищаем последнюю строку
    for (int x = 0; x < konsole_w; x++)
    {
        int offset = ((konsole_h - 1) * konsole_line_offset) + (x * 2);
        konsole_start[offset] = ' ';
        konsole_start[offset + 1] = konsole_current_color;
    }

    konsole_curr_y = konsole_h - 1;
}

// COLORS
void konsole_set_preambula_color()
{
    konsole_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
}

void konsole_set_good_result_color()
{
    konsole_set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
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
