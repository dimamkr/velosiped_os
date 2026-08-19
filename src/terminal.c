#include "konsole.h"
#include "terminal.h"
#include "keyboard.h"
#include "system.h"
#include "timer.h"
#include "types.h"
#include "datetime.h"
#include "ahci.h"

static char terminal_input_buff[256];
static int terminal_input_buff_lenght;
static bool terminal_EOI_flag;

static inline void terminal_buff_add_symbol(char symbol)
{
    terminal_input_buff[terminal_input_buff_lenght] = symbol;
    terminal_input_buff_lenght++;
}

// сюда ведет прерывание клавиатуры
void terminal_read_symbol(char symbol)
{
    switch (symbol)
    {
    case '\n':
        terminal_buff_add_symbol(0);
        terminal_EOI_flag = true;
        break;
    case '=':
        konsole_view_scroll_down();
        break;
    case '-':
        konsole_view_scroll_up();
        break;
    case '\b':
        if (terminal_input_buff_lenght > 0)
        {
            konsole_putch('\b');
            terminal_input_buff[--terminal_input_buff_lenght] = 0;
        }
        break;

    default:
        terminal_buff_add_symbol(symbol);
        konsole_putch(symbol);
    }
}

// получает ввод и выводит все символы кроме переноса строки
static inline void terminal_wait_for_input_line()
{
    terminal_input_buff_lenght = 0;
    terminal_EOI_flag = false;

    // каждое прерывание проверяем флаг
    while (!terminal_EOI_flag)
    {
        asm volatile("hlt");
    }

    konsole_println("");

    return;
}

void terminal_print_datetime()
{
    uint16_t year;
    uint8_t month, day, hour, minute, second;
    system_get_datetime(&year, &month, &day, &hour, &minute, &second);
    konsole_printf("%d-%d-%d %d:%d:%d",
                   year, month, day, hour, minute, second);
}

void terminal_print_time()
{
    uint32_t milis_time = timer_get_time();
    konsole_printf("%d%s%d%s\n", milis_time / 1000, " s ", milis_time % 1000, " ms");
}

void terminal_print_disks_info()
{
    dynamic_array_t *disks_info = ahci_enumerate_ports();

    if (disks_info->elements_count)
    {
        for (uint8_t i = 0;i < disks_info->elements_count;i++)
        {
            ahci_basic_identify_data_t *entry = dynamic_array_get_by_index(disks_info, i);

            konsole_printf("========\nPort: %d\nDevice type: ", entry->port_num);

            switch (entry->port_sig)
            {
                case SATA_SIG_ATA:
                    konsole_println("SATA drive");
                    break;
                case SATA_SIG_ATAPI:
                    konsole_println("SATAPI drive");
                    break;
                case SATA_SIG_SEMB:
                    konsole_println("Enclosure management bridge");
                    break;
                case SATA_SIG_PM:
                    konsole_println("Port mmultiplier");
                    break;
            }

            konsole_printf(
                "Model: %s\nSerial: %s\nSectors: %d\nLBA48|NCQ|DMA: %d|%d|%d\n",
                entry->model,
                entry->serial,
                entry->sectors,
                entry->lba48_supported,
                entry->ncq_supported,
                entry->dma_supported
            );
        }

        konsole_println("========");
    }
}

void terminal_handle_command_from_buff()
{
    if (strcmp(terminal_input_buff, "help"))
    {
        konsole_println("i can not help you");
    }
    else if (strcmp(terminal_input_buff, "time"))
    {
        terminal_print_time();
    }
    else if (strcmp(terminal_input_buff, "clear"))
    {
        konsole_clear();
    }
    else if (strcmp(terminal_input_buff, "ticks"))
    {
        char ticks[10] = {0};
        uint32_to_string(timer_get_ticks(), ticks);
        konsole_println(ticks);
    }
    else if (strcmp(terminal_input_buff, "datetime"))
    {
        terminal_print_datetime();
        konsole_printf("\n");
    }
    else if (strcmp(terminal_input_buff, "disks"))
    {
        terminal_print_disks_info();
    }
    else
    {
        konsole_println("i can not understand your text");
    }
}

void terminal_main_loop()
{
    konsole_println("");
    while (1)
    {
        terminal_print_datetime();
        konsole_print(">");

        terminal_wait_for_input_line();

        terminal_handle_command_from_buff();

        konsole_println("");
    }
}
