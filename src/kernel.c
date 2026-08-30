#include "konsole.h"
#include "terminal.h"
#include "idt_initialiser.h"
#include "gdt_initialiser.h"
#include "timer.h"
#include "keyboard.h"
#include "heap.h"
#include "ahci.h"
#include "disk.h"
#include "task.h"
#include "mbr.h"
#include "fat32.h"
#include "paging.h"
#include "bitmap.h"

void kernel_main_task(void *);
void kernel_continue(void);

// ------------------------------------------------------------
// Битовая карта физической памяти (глобальная)
// ------------------------------------------------------------

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
#define PRINT_FAIL                      \
    do                                  \
    {                                   \
        konsole_set_bad_result_color(); \
        konsole_println("Fail");        \
    } while (0)

__attribute__((section(".text.start"), cdecl)) void kernel_entry(void *disk_signature_addr)
{
    interrupt_disable();
    memcpy(_boot_disk_signature, disk_signature_addr, 6); // сохраняем сигнатуру диска для поиска

    // Инициализация менеджера физической памяти

    heap_init();

    konsole_init();
    konsole_set_good_result_color();

    konsole_print("\n\n");
    konsole_println("HEAP INITED");
    konsole_println("KONSOLE INITED");

    PRINT_INIT("Paging");
    init_paging();

    kernel_continue();

    __builtin_unreachable();
}

void kernel_continue(void)
{
    PRINT_OK;

    PRINT_INIT("GDT");
    gdt_init();
    PRINT_OK;

    PRINT_INIT("IDT");
    idt_init();
    PRINT_OK;

    PRINT_INIT("timer");
    timer_init(100);
    PRINT_OK;

    PRINT_INIT("keyboard");
    keyboard_init();
    PRINT_OK;

    PRINT_INIT("SCHEDULER");
    scheduler_init(kernel_main_task, NULL, STACK_SIZE_LARGE);
    scheduler_start();
}

void kernel_main_task(void *_)
{
    task_lock();
    PRINT_OK;

    interrupt_enable();
    task_unlock();

    PRINT_INIT("AHCI");
    if (ahci_init())
    {
        _ahci_supported = true;
        PRINT_OK;
    }
    else
    {
        _ahci_supported = false;
        PRINT_FAIL;

        konsole_set_warning_color();
        konsole_println("AHCI not found, using legacy mode");
    }

    // TODO режим отладки с кучей логов в консоль и сохранение в буфер логов
    // TODO история команд и того, что было на экране
    // TODO дамп памяти

    // for (int i = 1; i < 8; ++i)
    // {
    //     const char s[2] = {(char)i, 0};
    //     konsole_print(s);
    //     timer_wait(500);
    // }
    // konsole_println("")

    konsole_set_color(COLOR_LIGHT_BLUE, COLOR_BLACK);
    konsole_println("========");
    konsole_println("             .__               .__                  .___  ________    _________\n"
                    "___  __ ____ |  |   ____  _____|__|_____   ____   __| _/  \\_____  \\  /   _____/\n"
                    "\\  \\/ // __ \\|  |  /  _ \\/  ___/  \\____ \\_/ __ \\ / __ |    /   |   \\ \\_____  \\ "
                    "\n \\   /\\  ___/|  |_(  <_> )___ \\|  |  |_> >  ___// /_/ |   /    |    \\/        \\\n  "
                    "\\_/  \\___  >____/\\____/____  >__|   __/ \\___  >____ |   \\_______  /_______  /\n           "
                    "\\/                \\/   |__|        \\/     \\/           \\/        \\/ ");

    konsole_set_base_color();
    terminal_main_loop();
}