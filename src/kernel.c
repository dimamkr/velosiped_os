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
#include "framebuffer.h"
#include "colors.h"
#include "composer.h"
#include "pat.h"

#include <acpica/include/acpi.h>

void kernel_main_task(void *);

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

// composer_test.c — добавлять вызов в kernel_main_task после konsole_init
#include "composer.h"
#include "renderer.h"
#include "colors.h"
#include "font.h"

extern uint8_t *konsole_curr_font;
extern layer_t *konsole_layer;

void composer_test_alpha(void)
{
    // ============================================================
    // Слой 1: непрозрачный белый фон (z=1)
    // ============================================================
    layer_t *bg = composer_create_layer(50, 50, 500, 400, 1);
    renderer_draw_rect(bg, 0, 0, 500, 400, RGB(255, 255, 255));
    // bg имеет флаг LAYER_OPAQUE (по умолчанию в layer_init)

    // ============================================================
    // Слой 2: красный квадрат с alpha=255 (непрозрачный) (z=2)
    // ============================================================
    layer_t *red_opaque = composer_create_layer(80, 80, 150, 150, 2);
    renderer_draw_rect(red_opaque, 0, 0, 150, 150, RGBA(255, 0, 0, 255));
    red_opaque->flags &= ~LAYER_OPAQUE; // включаем blend

    // ============================================================
    // Слой 3: зелёный квадрат с alpha=128 (полупрозрачный) (z=3)
    // Перекрывает часть красного и часть белого фона.
    // Ожидаем:
    //   - на белом фоне:   (255, 128, 128) — светло-зелёный
    //   - на красном фоне: (128, 128, 0)   — оливковый
    // ============================================================
    layer_t *green_half = composer_create_layer(180, 180, 150, 150, 30);
    renderer_draw_rect(green_half, 0, 0, 150, 150, RGBA(0, 255, 0, 128));
    green_half->flags &= ~LAYER_OPAQUE;

    // ============================================================
    // Слой 4: синий квадрат с alpha=64 (почти прозрачный) (z=4)
    // Ожидаем: лёгкий голубой оттенок поверх того, что под ним.
    // ============================================================
    layer_t *blue_light = composer_create_layer(280, 280, 150, 150, 4);
    renderer_draw_rect(blue_light, 0, 0, 150, 150, RGBA(0, 0, 255, 64));
    blue_light->flags &= ~LAYER_OPAQUE;

    // ============================================================
    // Слой 5: "курсор" с per-pixel alpha (z=100)
    // Квадрат 32×32, где углы полностью прозрачные, центр — непрозрачный.
    // Проверяет, что в одном слое могут быть пиксели с разной alpha.
    // ============================================================
    layer_t *cursor = composer_create_layer(600, 400, 32, 32, 100);
    for (int y = 0; y < 32; y++)
    {
        for (int x = 0; x < 32; x++)
        {
            // Круг в центре: если расстояние от центра < 12 — непрозрачный,
            // если между 12 и 15 — полупрозрачный, иначе — прозрачный.
            int dx = x - 16;
            int dy = y - 16;
            int d2 = dx * dx + dy * dy;

            uint32_t color;
            if (d2 < 12 * 12)
                color = RGBA(255, 255, 255, 255); // центр — белый
            else if (d2 < 15 * 15)
                color = RGBA(255, 255, 255, 128); // край — полупрозрачный
            else
                color = RGBA(0, 0, 0, 0); // вне круга — прозрачный

            layer_put_pixel(cursor, x, y, color);
        }
    }
    cursor->flags &= ~LAYER_OPAQUE;

    // ============================================================
    // Слой 6: текст поверх всего (z=200)
    // Проверяет, что текст с непрозрачным фоном корректно перекрывает
    // полупрозрачные слои снизу.
    // ============================================================
    layer_t *text = composer_create_layer(80, 20, 400, 25, 200);
    renderer_draw_rect(text, 0, 0, 400, 25, RGB(0, 0, 0));

    const char *msg = "alpha test: 255 / 128 / 64 / per-pixel";
    int tx = 5;
    for (const char *s = msg; *s; s++)
    {
        renderer_draw_char(text, konsole_curr_font, *s,
                           tx, 5, RGB(255, 255, 255), RGB(0, 0, 0));
        tx += 8;
    }
    text->flags &= ~LAYER_OPAQUE;
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

    ACPI_STATUS result;

    PRINT_INIT("ACPI");
    konsole_println("");

    if (ACPI_FAILURE(result = AcpiInitializeSubsystem()))
    {
        PRINT_FAIL;
        konsole_set_warning_color();
        konsole_printf("AcpiInitializeSubsystem returned %x\n", result);
    }
    else if (ACPI_FAILURE(result = AcpiInitializeTables(NULL, 16, false)))
    {
        PRINT_FAIL;
        konsole_set_warning_color();
        konsole_printf("AcpiInitializeTables returned %x\n", result);
    }
    else if (ACPI_FAILURE(result = AcpiLoadTables()))
    {
        PRINT_FAIL;
        konsole_set_warning_color();
        konsole_printf("AcpiLoadTables returned %x\n", result);
    }
    else if (ACPI_FAILURE(result = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION)))
    {
        PRINT_FAIL;
        konsole_set_warning_color();
        konsole_printf("AcpiEnableSubsystem returned %x\n", result);
    }
    else if (ACPI_FAILURE(result = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION)))
    {
        PRINT_FAIL;
        konsole_set_warning_color();
        konsole_printf("AcpiInitializeObjects returned %x\n", result);
    }
    else
        PRINT_OK;

    task_unlock();

    PRINT_INIT("Terminal");
    terminal_init();
    PRINT_OK;

    // composer_test_alpha();

    // TODO режим отладки с кучей логов в консоль и сохранение в буфер логов
    // TODO история команд и того, что было на экране
    // TODO дамп памяти

    // TODO отдельная задача для нижних обработчиков прерываний
    // TODO сделать нормальный по распределению приоритетов планировщик

    // TODO пользовательские процессы из 3 кольца и безопасность

    // TODO framebuffer_flush раз в заданное время. также написать удобную функцию для обновления чего либо раз в фиксированное время

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