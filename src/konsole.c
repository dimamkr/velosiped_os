#include "konsole.h"
#include "dynamic_array.h"
#include "task.h"
#include "ram.h"

// адрес начала видеопамяти
konsole_symbol_t *konsole_start = (konsole_symbol_t *)VIDEO_MEMORY_START;

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
    TASK_LOCKED_FUNCTION;

    konsole_output_history_start_index = 0;

    while (konsole_output_history->elements_count > KONSOLE_W * KONSOLE_H)
    {
        dynamic_array_pop_front(konsole_output_history);
    }

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
    TASK_LOCKED_FUNCTION;
    for (int i = 0; text[i] != 0; ++i)
    {
        konsole_putch(text[i]);
    }
}

void konsole_println(const char *text)
{
    TASK_LOCKED_FUNCTION;

    konsole_print(text);
    konsole_print("\n");
}

/*
 * ============================================================
 * Kernel printf
 *
 * Поддерживается:
 *
 *   %%      %
 *   %c      char
 *   %s      string
 *   %d      signed decimal (32 bit)
 *   %i      signed decimal (32 bit)
 *   %u      unsigned decimal (32 bit)
 *   %x      hex (32 bit)
 *   %X      HEX (32 bit)
 *   %p      pointer (32 bit)
 *   %llx    64-bit hex
 *   %llX    64-bit HEX
 *
 * Flags:
 *   -
 *   +
 *   space
 *   0
 *   #
 *
 * Width:
 *   number
 *   *
 *
 * Precision:
 *   .number
 *   .*
 *
 * Length:
 *   h
 *   hh
 *   l
 *   ll
 *   z
 *   j
 *   t
 *
 * НЕ поддерживается:
 *   %lld
 *   %lli
 *   %llu
 *   %lld / %llu
 *   floating point
 *   %n
 *
 * Вся арифметика форматтера — 32-bit.
 * Для %llx/%llX 64-bit число разбирается как:
 *
 *      high uint32_t
 *      low  uint32_t
 *
 * ============================================================
 */

typedef __builtin_va_list konsole_va_list;

/*
 * ============================================================
 * 32-bit unsigned integer
 * ============================================================
 */

static void konsole_put_uint32(
    uint32_t value,
    uint32_t base,
    bool_t uppercase,
    int width,
    int precision,
    bool_t left,
    bool_t zero_pad,
    bool_t alternate,
    char sign)
{
    const char *digits;

    if (uppercase)
        digits = "0123456789ABCDEF";
    else
        digits = "0123456789abcdef";

    char buffer[32];
    int len = 0;

    /*
     * printf semantics:
     *
     * %.0d -> empty if value == 0
     */
    if (value == 0 && precision == 0)
    {
        len = 0;
    }
    else
    {
        do
        {
            uint32_t digit = value % base;

            buffer[len++] = digits[digit];

            value /= base;
        } while (value != 0);
    }

    /*
     * Precision = minimum number of digits.
     */
    while (len < precision)
        buffer[len++] = '0';

    /*
     * Prefix.
     */
    int prefix_len = 0;

    if (alternate && len > 0)
    {
        if (base == 16)
            prefix_len = 2;
        else if (base == 8)
            prefix_len = 1;
    }

    int sign_len = sign ? 1 : 0;

    int total =
        sign_len +
        prefix_len +
        len;

    /*
     * '0' flag is ignored if precision is present.
     */
    bool_t use_zero =
        zero_pad &&
        !left &&
        precision < 0;

    /*
     * Special handling of:
     *
     *     -00001234
     *     0x00001234
     */
    if (use_zero)
    {
        if (sign)
            konsole_putch(sign);

        if (prefix_len)
        {
            if (base == 16)
            {
                konsole_putch('0');
                konsole_putch(uppercase ? 'X' : 'x');
            }
            else if (base == 8)
            {
                konsole_putch('0');
            }
        }

        for (int i = total; i < width; i++)
            konsole_putch('0');

        while (len > 0)
            konsole_putch(buffer[--len]);

        return;
    }

    /*
     * Left padding.
     */
    if (!left)
    {
        for (int i = total; i < width; i++)
            konsole_putch(' ');
    }

    /*
     * Sign.
     */
    if (sign)
        konsole_putch(sign);

    /*
     * Prefix.
     */
    if (prefix_len)
    {
        if (base == 16)
        {
            konsole_putch('0');
            konsole_putch(uppercase ? 'X' : 'x');
        }
        else if (base == 8)
        {
            konsole_putch('0');
        }
    }

    /*
     * Number.
     */
    while (len > 0)
        konsole_putch(buffer[--len]);

    /*
     * Right padding.
     */
    if (left)
    {
        for (int i = total; i < width; i++)
            konsole_putch(' ');
    }
}

/*
 * ============================================================
 * 32-bit signed integer
 * ============================================================
 */

static void konsole_put_int32(
    int32_t value,
    int width,
    int precision,
    bool_t left,
    bool_t zero_pad,
    bool_t plus,
    bool_t space)
{
    char sign = 0;
    uint32_t magnitude;

    if (value < 0)
    {
        sign = '-';

        /*
         * Безопасно для INT32_MIN.
         */
        magnitude =
            (uint32_t)(-(value + 1));

        magnitude++;
    }
    else
    {
        magnitude = (uint32_t)value;

        if (plus)
            sign = '+';
        else if (space)
            sign = ' ';
    }

    konsole_put_uint32(
        magnitude,
        10,
        false,
        width,
        precision,
        left,
        zero_pad,
        false,
        sign);
}

/*
 * ============================================================
 * String
 * ============================================================
 */

static void konsole_put_string(
    const char *str,
    int width,
    int precision,
    bool_t left)
{
    if (str == NULL)
        str = "(null)";

    int len = 0;

    while (str[len] != '\0')
    {
        if (precision >= 0 && len >= precision)
            break;

        len++;
    }

    if (!left)
    {
        for (int i = len; i < width; i++)
            konsole_putch(' ');
    }

    for (int i = 0; i < len; i++)
        konsole_putch(str[i]);

    if (left)
    {
        for (int i = len; i < width; i++)
            konsole_putch(' ');
    }
}

/*
 * ============================================================
 * 64-bit HEX
 *
 * НИКАКОЙ 64-bit arithmetic.
 *
 * Значение передаётся как:
 *
 *     high
 *     low
 *
 * и выводится nibble-by-nibble.
 * ============================================================
 */

static void konsole_put_hex64(
    uint32_t high,
    uint32_t low,
    bool_t uppercase,
    int width,
    int precision,
    bool_t left,
    bool_t alternate)
{
    const char *digits;

    if (uppercase)
        digits = "0123456789ABCDEF";
    else
        digits = "0123456789abcdef";

    /*
     * Максимум 16 hex digits.
     */
    char buffer[16];

    int len = 0;

    /*
     * Сначала low.
     *
     * Получаем hex цифры справа налево.
     */
    uint32_t value = low;

    while (value != 0)
    {
        buffer[len++] =
            digits[value & 0xF];

        value >>= 4;
    }

    /*
     * Потом high.
     */
    value = high;

    while (value != 0)
    {
        buffer[len++] =
            digits[value & 0xF];

        value >>= 4;
    }

    /*
     * Zero.
     */
    if (len == 0 && precision != 0)
        buffer[len++] = '0';

    /*
     * Precision.
     */
    while (len < precision)
        buffer[len++] = '0';

    /*
     * Prefix.
     */
    int prefix_len =
        alternate && len > 0 ? 2 : 0;

    int total =
        len + prefix_len;

    /*
     * Left padding.
     */
    if (!left)
    {
        for (int i = total; i < width; i++)
            konsole_putch(' ');
    }

    /*
     * Prefix.
     */
    if (prefix_len)
    {
        konsole_putch('0');
        konsole_putch(
            uppercase ? 'X' : 'x');
    }

    /*
     * Reverse buffer.
     *
     * В buffer цифры лежат справа налево,
     * поэтому выводим с конца.
     */
    while (len > 0)
        konsole_putch(buffer[--len]);

    /*
     * Right padding.
     */
    if (left)
    {
        for (int i = total; i < width; i++)
            konsole_putch(' ');
    }
}

/*
 * ============================================================
 * printf parser
 * ============================================================
 */

void konsole_vprintf(
    const char *format,
    konsole_va_list args)
{
    TASK_LOCKED_FUNCTION;

    while (*format)
    {
        /*
         * Обычный символ.
         */
        if (*format != '%')
        {
            konsole_putch(*format);
            format++;
            continue;
        }

        format++;

        /*
         * %%
         */
        if (*format == '%')
        {
            konsole_putch('%');
            format++;
            continue;
        }

        /*
         * ====================================================
         * FLAGS
         * ====================================================
         */

        bool_t left = false;
        bool_t plus = false;
        bool_t space = false;
        bool_t zero = false;
        bool_t alternate = false;

        for (;;)
        {
            switch (*format)
            {
            case '-':
                left = true;
                format++;
                continue;

            case '+':
                plus = true;
                format++;
                continue;

            case ' ':
                space = true;
                format++;
                continue;

            case '0':
                zero = true;
                format++;
                continue;

            case '#':
                alternate = true;
                format++;
                continue;

            default:
                goto flags_done;
            }
        }

    flags_done:

        /*
         * ====================================================
         * WIDTH
         * ====================================================
         */

        int width = 0;

        if (*format == '*')
        {
            width =
                __builtin_va_arg(args, int);

            format++;

            if (width < 0)
            {
                left = true;
                width = -width;
            }
        }
        else
        {
            while (*format >= '0' &&
                   *format <= '9')
            {
                width =
                    width * 10 +
                    (*format - '0');

                format++;
            }
        }

        /*
         * ====================================================
         * PRECISION
         * ====================================================
         */

        int precision = -1;

        if (*format == '.')
        {
            format++;

            if (*format == '*')
            {
                precision =
                    __builtin_va_arg(args, int);

                format++;

                if (precision < 0)
                    precision = -1;
            }
            else
            {
                precision = 0;

                while (*format >= '0' &&
                       *format <= '9')
                {
                    precision =
                        precision * 10 +
                        (*format - '0');

                    format++;
                }
            }

            /*
             * 0 flag ignored when precision exists.
             */
            zero = false;
        }

        /*
         * ====================================================
         * LENGTH
         * ====================================================
         */

        enum
        {
            LENGTH_NONE,
            LENGTH_HH,
            LENGTH_H,
            LENGTH_L,
            LENGTH_LL,
            LENGTH_Z,
            LENGTH_J,
            LENGTH_T
        } length = LENGTH_NONE;

        if (*format == 'h')
        {
            format++;

            if (*format == 'h')
            {
                length = LENGTH_HH;
                format++;
            }
            else
            {
                length = LENGTH_H;
            }
        }
        else if (*format == 'l')
        {
            format++;

            if (*format == 'l')
            {
                length = LENGTH_LL;
                format++;
            }
            else
            {
                length = LENGTH_L;
            }
        }
        else if (*format == 'z')
        {
            length = LENGTH_Z;
            format++;
        }
        else if (*format == 'j')
        {
            length = LENGTH_J;
            format++;
        }
        else if (*format == 't')
        {
            length = LENGTH_T;
            format++;
        }

        /*
         * ====================================================
         * SPECIFIER
         * ====================================================
         */

        switch (*format)
        {
            /*
             * ------------------------------------------------
             * %d / %i
             * ------------------------------------------------
             */

        case 'd':
        case 'i':
        {
            /*
             * 64-bit signed decimal специально
             * НЕ поддерживается.
             */
            if (length == LENGTH_LL)
            {
                /*
                 * Аргумент всё равно надо забрать,
                 * чтобы va_list не сломался.
                 *
                 * Мы НЕ выполняем с ним арифметику.
                 */
                __builtin_va_arg(
                    args,
                    long long);

                konsole_put_string(
                    "[%lld unsupported]",
                    0,
                    -1,
                    false);

                break;
            }

            int32_t value;

            if (length == LENGTH_L)
            {
                value =
                    (int32_t)__builtin_va_arg(
                        args,
                        long);
            }
            else
            {
                value =
                    (int32_t)__builtin_va_arg(
                        args,
                        int);
            }

            konsole_put_int32(
                value,
                width,
                precision,
                left,
                zero,
                plus,
                space);

            break;
        }

            /*
             * ------------------------------------------------
             * %u
             * ------------------------------------------------
             */

        case 'u':
        {
            /*
             * 64-bit decimal не реализуем.
             */
            if (length == LENGTH_LL)
            {
                __builtin_va_arg(
                    args,
                    unsigned long long);

                konsole_put_string(
                    "[%llu unsupported]",
                    0,
                    -1,
                    false);

                break;
            }

            uint32_t value;

            if (length == LENGTH_L)
            {
                value =
                    (uint32_t)__builtin_va_arg(
                        args,
                        unsigned long);
            }
            else
            {
                value =
                    (uint32_t)__builtin_va_arg(
                        args,
                        unsigned int);
            }

            konsole_put_uint32(
                value,
                10,
                false,
                width,
                precision,
                left,
                zero,
                false,
                0);

            break;
        }

            /*
             * ------------------------------------------------
             * %x / %X
             * ------------------------------------------------
             */

        case 'x':
        case 'X':
        {
            bool_t uppercase =
                (*format == 'X');

            /*
             * %llx / %llX
             *
             * 64-bit arithmetic НЕ используется.
             */
            if (length == LENGTH_LL)
            {
                unsigned long long value =
                    __builtin_va_arg(
                        args,
                        unsigned long long);

                /*
                 * Разбиваем 64-bit value на
                 * два 32-bit слова.
                 *
                 * GCC понимает это как extract halves.
                 */
                uint32_t low =
                    (uint32_t)value;

                uint32_t high =
                    (uint32_t)(value >> 32);

                konsole_put_hex64(
                    high,
                    low,
                    uppercase,
                    width,
                    precision,
                    left,
                    alternate);

                break;
            }

            uint32_t value;

            if (length == LENGTH_L)
            {
                value =
                    (uint32_t)__builtin_va_arg(
                        args,
                        unsigned long);
            }
            else
            {
                value =
                    (uint32_t)__builtin_va_arg(
                        args,
                        unsigned int);
            }

            konsole_put_uint32(
                value,
                16,
                uppercase,
                width,
                precision,
                left,
                zero,
                alternate,
                0);

            break;
        }

            /*
             * ------------------------------------------------
             * %o
             * ------------------------------------------------
             */

        case 'o':
        {
            /*
             * Только 32-bit.
             */
            uint32_t value =
                (uint32_t)__builtin_va_arg(
                    args,
                    unsigned int);

            konsole_put_uint32(
                value,
                8,
                false,
                width,
                precision,
                left,
                zero,
                alternate,
                0);

            break;
        }

            /*
             * ------------------------------------------------
             * %c
             * ------------------------------------------------
             */

        case 'c':
        {
            int value =
                __builtin_va_arg(
                    args,
                    int);

            if (!left)
            {
                for (int i = 1; i < width; i++)
                    konsole_putch(' ');
            }

            konsole_putch((char)value);

            if (left)
            {
                for (int i = 1; i < width; i++)
                    konsole_putch(' ');
            }

            break;
        }

            /*
             * ------------------------------------------------
             * %s
             * ------------------------------------------------
             */

        case 's':
        {
            const char *str =
                __builtin_va_arg(
                    args,
                    const char *);

            konsole_put_string(
                str,
                width,
                precision,
                left);

            break;
        }

            /*
             * ------------------------------------------------
             * %p
             * ------------------------------------------------
             */

        case 'p':
        {
            uint32_t value =
                (uint32_t)__builtin_va_arg(
                    args,
                    void *);

            /*
             * 32-bit kernel:
             *
             * 0xXXXXXXXX
             */
            konsole_putch('0');
            konsole_putch('x');

            konsole_put_uint32(
                value,
                16,
                false,
                8,
                8,
                false,
                true,
                false,
                0);

            break;
        }

            /*
             * ------------------------------------------------
             * неизвестный specifier
             * ------------------------------------------------
             */

        default:
        {
            konsole_putch('%');

            if (*format)
                konsole_putch(*format);

            break;
        }
        }

        if (*format)
            format++;
    }
}

/*
 * ============================================================
 * Public printf
 * ============================================================
 */

void konsole_printf(
    const char *format,
    ...)
{
    konsole_va_list args;

    __builtin_va_start(args, format);

    konsole_vprintf(
        format,
        args);

    __builtin_va_end(args);
}

/*
__attribute__((__format__(__printf__, 1, 2)))
void konsole_printf(const char *format, ...)
{
    TASK_LOCKED_FUNCTION;

    va_list args;
    va_start(args, format);

    konsole_vprintf(format, args);

    va_end(args);
}*/

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
