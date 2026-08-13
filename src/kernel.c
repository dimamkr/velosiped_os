#include "konsole.h"
#include "terminal.h"
#include "idt_initialiser.h"
#include "gdt_initialiser.h"
#include "timer.h"
#include "keyboard.h"
#include "heap.h"

#define PRINT_INIT(x)                   \
    do                                  \
    {                                   \
        konsole_set_preambula_color();  \
        konsole_print("Initializing "); \
        konsole_print(x);               \
        konsole_print("...");           \
    } while (0)
#define PRINT_OK                         \
    do                                   \
    {                                    \
        konsole_set_good_result_color(); \
        konsole_println("OK");           \
    } while (0)

__attribute__((section(".text.start"))) void kernel_main(void)
{
    konsole_set_base_color();
    konsole_println("\nWELCOME TO KERNEL");

    PRINT_INIT("GDT");
    gdt_init();
    PRINT_OK;

    PRINT_INIT("IDT");
    idt_init();
    PRINT_OK;

    PRINT_INIT("timer");
    timer_init(2500);
    PRINT_OK;

    PRINT_INIT("keyboard");
    keyboard_init();
    PRINT_OK;

    PRINT_INIT("heap");
    heap_init();
    PRINT_OK;

    interrupt_enable();

    uint32_t size = 30 * MB;
    char *a = malloc(size);
    char *b = malloc(size);

    terminal_print_time();
    memcpy(a, b, size);
    terminal_print_time();

    // TODO std::vector
    // TODO куча должна быть в области данных
    // TODO режим отладки с кучей логов в консоль и сохранение в буфер логов
    // TODO история команд и того, что было на экране
    // TODO листать вверх-вниз вывод консоли
    // TODO дамп памяти и HEAP MAGIC
    // TODO увеличение размера кучи

    konsole_set_base_color();

    konsole_set_color(COLOR_GREEN, COLOR_LIGHT_BLUE);
    konsole_printf("%s%d%s\n", "СОСАЛИ ", 100, " ХУЕВ");

    konsole_set_base_color();
    terminal_main_loop();
}