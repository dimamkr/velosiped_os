#include "konsole.h"
#include "terminal.h"
#include "keyboard.h"
#include "task_event.h"
#include "system.h"
#include "string.h"
#include "timer.h"
#include "types.h"
#include "datetime.h"
#include "ahci.h"
#include "fat32.h"
#include "task.h"
#include "argparse.h"
#include "hash_table.h"

static char terminal_input_buff[256];
static int terminal_input_buff_lenght;
static bool_t terminal_EOI_flag;

fat32_info_t info;
fat32_basic_file_info_t root;
dynamic_array_t *path;

hash_table_t *cmd_handlers;

static inline void terminal_buff_add_symbol(char symbol)
{
    terminal_input_buff[terminal_input_buff_lenght] = symbol;
    terminal_input_buff_lenght++;
}

void terminal_process_keyboard_events(void)
{
    keyboard_event_t ev;
    while (keyboard_dequeue_event(&ev))
    {
        // Игнорируем отпускания
        if (!ev.pressed)
            continue;

        // Ctrl+↑ / Ctrl+↓ для прокрутки
        if (ev.modifiers & MOD_CTRL)
        {
            if (ev.keycode == KEY_UP)
            {
                konsole_view_scroll_up();
                continue;
            }
            else if (ev.keycode == KEY_DOWN)
            {
                konsole_view_scroll_down();
                continue;
            }
        }

        // Обработка специальных клавиш
        switch (ev.keycode)
        {
        case KEY_BACKSPACE:
            if (terminal_input_buff_lenght > 0)
            {
                konsole_putch('\b');
                terminal_input_buff[--terminal_input_buff_lenght] = 0;
            }
            break;
        case KEY_ENTER:
            terminal_buff_add_symbol(0);
            terminal_EOI_flag = true;
            break;
        case KEY_SPACE:
            terminal_buff_add_symbol(' ');
            konsole_putch(' ');
            break;
        default:
            if (ev.keycode < 0x80)
            { // ASCII
                char ch = (char)ev.keycode;
                terminal_buff_add_symbol(ch);
                konsole_putch(ch);
            }
            // Остальные спецклавиши игнорируем
            break;
        }
    }
}

// получает ввод и выводит все символы кроме переноса строки
static inline void terminal_wait_for_input_line()
{
    terminal_input_buff_lenght = 0;
    terminal_EOI_flag = false;

    while (!terminal_EOI_flag)
    {
        task_wait_until(keyboard_event);
        terminal_process_keyboard_events();
    }

    konsole_println("");
}

void terminal_register_command_handler(char *command, terminal_command_handler_cb handler)
{
    hash_table_insert(cmd_handlers, command, strlen(command), &handler, sizeof(terminal_command_handler_cb));
}

// --------- Обработчики команд ---------

void terminal_print_help(argparse_command_t *command)
{
    konsole_println("Avaliable commands: ");
}

void terminal_clear(argparse_command_t *command)
{
    konsole_clear();
}

void terminal_print_datetime(argparse_command_t *command)
{
    datetime_t dt;

    system_get_datetime(&dt);
    konsole_printf("%d-%d-%d %d:%d:%d",
                   dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
}

void terminal_print_time(argparse_command_t *command)
{
    uint32_t milis_time = timer_get_time();
    konsole_printf("%d%s%d%s\n", milis_time / 1000, " s ", milis_time % 1000, " ms");
}

void terminal_print_ticks(argparse_command_t *command)
{
    konsole_printf("%d\n", timer_get_ticks());
}

void terminal_print_disks(argparse_command_t *command)
{
    dynamic_array_t *disks_info = ahci_enumerate_ports();

    if (disks_info->elements_count)
    {
        for (uint8_t i = 0; i < disks_info->elements_count; i++)
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
                (uint32_t)entry->sectors,
                entry->lba48_supported,
                entry->ncq_supported,
                entry->dma_supported);
        }

        konsole_println("========");
    }
    else
        konsole_println("No disks found");

    dynamic_array_destroy(disks_info);
}

void terminal_print_path(argparse_command_t *command)
{
    for (uint8_t i = 0;i < path->elements_count;i++)
    {
        fat32_basic_file_info_t *dir = dynamic_array_get_by_index(path, i);

        konsole_printf("%s/", dir->filename);
    }
}

void terminal_print_listdir(argparse_command_t *command)
{
    dynamic_array_t *listdir = fat32_read_directory(&info, dynamic_array_get_top(path));

    // konsole_printf("%x\n", listdir);

    if (listdir->elements_count != 0)
        konsole_println("========");

    for (uint32_t i = 0;i < listdir->elements_count;i++)
    {
        fat32_basic_file_info_t *file_info = dynamic_array_get_by_index(listdir, i);

        konsole_printf("Filename: %s\n", file_info->filename);

        konsole_print("Attributes: ");

        if (file_info->attributes & FAT32_ATTRIBUTE_ARCHIVE)
            konsole_print("Archived ");
        if (file_info->attributes & FAT32_ATTRIBUTE_DIRECTORY)
            konsole_print("Directory ");
        if (file_info->attributes & FAT32_ATTRIBUTE_HIDDEN)
            konsole_print("Hidden ");
        if (file_info->attributes & FAT32_ATTRIBUTE_READONLY)
            konsole_print("Readonly ");
        if (file_info->attributes & FAT32_ATTRIBUTE_SYSTEM)
            konsole_print("System ");
        if (file_info->attributes & FAT32_ATTRIBUTE_VOLUME_ID)
            konsole_print("VolumeID ");

        konsole_println("");

        datetime_t dt;
        datetime_datetime_from_fat(&(file_info->creation_datetime), &dt);
        konsole_printf("Created at: %d-%d-%d %d:%d:%d\n", dt.day, dt.month, dt.year, dt.hour, dt.minute, dt.second);
        datetime_datetime_from_fat(&(file_info->last_modify_datetime), &dt);
        konsole_printf("Modified at: %d-%d-%d %d:%d:%d\n", dt.day, dt.month, dt.year, dt.hour, dt.minute, dt.second);

        if (!(file_info->attributes & (FAT32_ATTRIBUTE_DIRECTORY | FAT32_ATTRIBUTE_VOLUME_ID)))
            konsole_printf("Size: %d B\n", file_info->size);

        konsole_println("========");
    }
}

void terminal_view(argparse_command_t *command)
{
    
}

// --------- Хэндлер ---------

void terminal_handle_command_from_buff()
{
    argparse_command_t command;
    argparse_parse_command(terminal_input_buff, &command);

    terminal_command_handler_cb *handler = hash_table_get(cmd_handlers, command.command_name, strlen(command.command_name));
    
    if (handler)
        (*handler)(&command);
    else
        konsole_println("Unknown command");

    argparse_free_command(&command);
}

void terminal_main_loop()
{
    fat32_get_bootable_partition_info_sync(&info);
    path = dynamic_array_create(sizeof(fat32_basic_file_info_t));
    fat32_basic_file_info_t root;
    fat32_mount(&info, "ROOT", &root);
    dynamic_array_push_back(path, &root);

    cmd_handlers = hash_table_create();

    terminal_register_command_handler("help", terminal_print_help);
    terminal_register_command_handler("time", terminal_print_time);
    terminal_register_command_handler("clear", terminal_clear);
    terminal_register_command_handler("ticks", terminal_print_ticks);
    terminal_register_command_handler("datetime", terminal_print_datetime);
    terminal_register_command_handler("disks", terminal_print_disks);
    terminal_register_command_handler("listdir", terminal_print_listdir);

    konsole_println("");

    while (1)
    {
        terminal_print_path(NULL);
        konsole_print(">");

        terminal_wait_for_input_line();

        terminal_handle_command_from_buff();

        konsole_println("");
    }
}
