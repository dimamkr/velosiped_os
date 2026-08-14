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
    heap_init();

    konsole_init();
    konsole_set_good_result_color();
    konsole_println("\nHEAP INITED");
    konsole_println("KONSOLE INITED");
    konsole_set_good_result_color();

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

    interrupt_enable();

    // TODO режим отладки с кучей логов в консоль и сохранение в буфер логов
    // TODO история команд и того, что было на экране
    // TODO листать вверх-вниз вывод консоли
    // TODO дамп памяти

    // for (int i = 1; i < 8; ++i)
    // {
    //     const char s[2] = {(char)i, 0};
    //     konsole_print(s);
    //     timer_wait(500);
    // }
    // konsole_println("");

    konsole_set_color(COLOR_LIGHT_BLUE, COLOR_BLACK);

    konsole_println("             .__               .__                  .___  ________    _________\n"
                    "___  __ ____ |  |   ____  _____|__|_____   ____   __| _/  \\_____  \\  /   _____/\n"
                    "\\  \\/ // __ \\|  |  /  _ \\/  ___/  \\____ \\_/ __ \\ / __ |    /   |   \\ \\_____  \\ "
                    "\n \\   /\\  ___/|  |_(  <_> )___ \\|  |  |_> >  ___// /_/ |   /    |    \\/        \\\n  "
                    "\\_/  \\___  >____/\\____/____  >__|   __/ \\___  >____ |   \\_______  /_______  /\n           "
                    "\\/                \\/   |__|        \\/     \\/           \\/        \\/ ");

    konsole_set_base_color();
    terminal_main_loop();
}