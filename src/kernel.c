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
#include "vmm.h"
#include "pmm.h"
#include "pio.h"
#include "framebuffer.h"
#include "colors.h"
#include "composer.h"
#include "pat.h"
#include "mouse.h"
#include "power.h"
#include "sysenter.h"


void kernel_main_task(void *);


__attribute__((section(".text.start"), cdecl)) void kernel_entry(void *param)
{
    interrupt_disable();
    memcpy(_boot_disk_signature, param, 6); // сохраняем сигнатуру диска для поиска

    framebuffer_read_boot_info();

    heap_init();

    gdt_init();

    idt_init();

    pmm_init();

    pat_init();

    vmm_init();

    colors_init();

    framebuffer_init();

    timer_init(100);

    keyboard_init();

    scheduler_init(kernel_main_task, NULL, STACK_SIZE_LARGE);
    scheduler_start();
}

// к этому моменту должны быть настроены все прерывания
// они автоматически разрешены из-за начального стека задачи
void kernel_main_task(void *_)
{
    task_lock();

    composer_init();

    konsole_init();

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

    PRINT_INIT("PIO");
    pio_init();
    PRINT_OK;

    disk_init();

    power_init();

    task_unlock();

    PRINT_INIT("mouse");
    mouse_init();
    PRINT_OK;

    PRINT_INIT("Terminal");
    terminal_init();
    PRINT_OK;

    PRINT_INIT("SYSENTER");
    sysenter_init();
    PRINT_OK;

    // TODO режим отладки с кучей логов в консоль и сохранение в буфер логов
    // TODO история команд и того, что было на экране
    // TODO дамп памяти

    // TODO отдельная задача для нижних обработчиков прерываний
    // TODO сделать нормальный по распределению приоритетов планировщик

    // TODO пользовательские процессы из 3 кольца и безопасность

    // for (int i = 1; i < 8; ++i)
    // {
    //     const char s[2] = {(char)i, 0};
    //     konsole_print(s);
    //     timer_wait(500);
    // }
    // konsole_println("")

    konsole_set_info_color();
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