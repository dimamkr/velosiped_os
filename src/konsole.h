#ifndef KONSOLE
#define KONSOLE

#include "types.h"
#include <stdarg.h>

#define KONSOLE_W 80
#define KONSOLE_H 25

void konsole_init();
void konsole_clear();
void konsole_putch(char ch);
void konsole_print(const char *text);
void konsole_println(const char *text);
void konsole_set_color(uint32_t fg, uint32_t bg);
void konsole_scroll_down();
void konsole_vprintf(const char *format, va_list args);
void konsole_printf(const char *format, ...);
bool konsole_view_scroll_up();
bool konsole_view_scroll_down();
void konsole_redraw_from_history();

void konsole_set_preambula_color();
void konsole_set_good_result_color();
void konsole_set_warning_color();
void konsole_set_bad_result_color();
void konsole_set_panic_color();
void konsole_set_base_color();
void konsole_set_info_color();

typedef struct
{
    byte_t symbol;
    uint32_t fg_color;
    uint32_t bg_color;
} konsole_symbol_t;

#endif
