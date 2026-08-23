#ifndef KONSOLE
#define KONSOLE

#include "system.h"
#include <stdarg.h>
#include "types.h"
#include "dynamic_array.h"
#include "task.h"

#define COLOR_BLACK 0x0
#define COLOR_BLUE 0x1
#define COLOR_GREEN 0x2
#define COLOR_CYAN 0x3
#define COLOR_RED 0x4
#define COLOR_MAGENTA 0x5
#define COLOR_BROWN 0x6
#define COLOR_LIGHT_GRAY 0x7
#define COLOR_DARK_GRAY 0x8
#define COLOR_LIGHT_BLUE 0x9
#define COLOR_LIGHT_GREEN 0xA
#define COLOR_LIGHT_CYAN 0xB
#define COLOR_LIGHT_RED 0xC
#define COLOR_LIGHT_MAGENTA 0xD
#define COLOR_YELLOW 0xE
#define COLOR_WHITE 0xF

#define KONSOLE_W 80
#define KONSOLE_H 25

void konsole_init();
void konsole_clear();
void konsole_putch(char ch);
void konsole_print(const char *text);
void konsole_println(const char *text);
void konsole_set_color(uint32_t fg, uint32_t bg);
void konsole_scroll_down();
void konsole_printf(const char *format, ...);
void konsole_printfln(const char *format, ...);
bool konsole_view_scroll_up();
bool konsole_view_scroll_down();

void konsole_set_preambula_color();
void konsole_set_good_result_color();
void konsole_set_warning_color();
void konsole_set_bad_result_color();
void konsole_set_panic_color();
void konsole_set_base_color();

typedef struct
{
    byte_t symbol;
    // первые 4 бита задний фон, вторые 4 цвет символа
    byte_t colors;
} __attribute__((packed)) konsole_symbol_t;

// typedef struct
// {
//     konsole_symbol_t line[KONSOLE_W];
// } konsole_line_t;

#endif
