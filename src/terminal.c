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
static bool_t terminal_cancelled_flag;

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
            else if (LOWER(ev.keycode) == 'c')
            {
                terminal_buff_add_symbol(0);
                terminal_cancelled_flag = true;
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
static inline const char *terminal_get_input_line()
{
    terminal_input_buff_lenght = 0;
    terminal_EOI_flag = false;
    terminal_cancelled_flag = false;

    while (!terminal_EOI_flag && !terminal_cancelled_flag)
    {
        task_wait_until(keyboard_event);
        terminal_process_keyboard_events();
    }

    if (terminal_EOI_flag)
    {
        konsole_println("");
        return terminal_input_buff;
    }
    
    return NULL;
}

void terminal_register_command_handler(char *command, terminal_command_handler_cb handler)
{
    hash_table_insert(cmd_handlers, command, strlen(command) + 1, &handler, sizeof(terminal_command_handler_cb));
}

// --------- Обработчики команд ---------

bool_t terminal_print_help(argparse_command_t *command)
{
    konsole_print("Avaliable commands: ");
    bool_t comma = false;

    for (uint32_t i = 0;i < primes[cmd_handlers->buff_count_index];i++)
    {
        for (linked_list_node_t *node = *(cmd_handlers->key_buff_root + i);node;node = node->right)
        {
            unsigned char *key = node->value;

            if (comma)
                konsole_print(", ");
            comma = true;

            konsole_print(key);
        }
    }

    konsole_println("");

    return true;
}

bool_t terminal_clear(argparse_command_t *command)
{
    konsole_clear();

    return true;
}

bool_t terminal_print_datetime(argparse_command_t *command)
{
    datetime_t dt;

    datetime_get(&dt);
    konsole_printf("%d-%d-%d %d:%d:%d",
                   dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

    return true;
}

bool_t terminal_print_time(argparse_command_t *command)
{
    uint32_t milis_time = timer_get_time();
    konsole_printf("%d%s%d%s\n", milis_time / 1000, " s ", milis_time % 1000, " ms");

    return true;
}

bool_t terminal_print_ticks(argparse_command_t *command)
{
    konsole_printf("%d\n", timer_get_ticks());

    return true;
}

bool_t terminal_print_disks(argparse_command_t *command)
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
    {
        konsole_println("No disks found");
        dynamic_array_destroy(disks_info);
        return false;
    }

    dynamic_array_destroy(disks_info);

    return true;
}

bool_t terminal_print_path(argparse_command_t *command)
{
    for (uint8_t i = 0;i < path->elements_count;i++)
    {
        fat32_basic_file_info_t *dir = dynamic_array_get_by_index(path, i);

        konsole_printf("%s/", dir->filename);
    }

    return true;
}

bool_t terminal_print_listdir(argparse_command_t *command)
{
    dynamic_array_t *listdir;
    char *pattern = NULL;
    bool_t ignore_case = false;

    for (uint32_t i = 0;i < command->arguments->elements_count;i++)
    {
        argparse_argument_t *arg = dynamic_array_get_by_index(command->arguments, i);

        if (arg->value == 0)
            pattern = arg->name;
        else if (strcmp(arg->name, "opt") == 0)
        {
            if (strcmp(arg->value, "ignore-case") == 0)
                ignore_case = true;
            else
            {
                konsole_printf("Error: invalid option '%s'\n", arg->value);
                return false;
            }
        }
    }

    listdir = fat32_find_files(&info, dynamic_array_get_top(path), pattern, ignore_case);

    if (listdir == NULL)
    {
        konsole_println("Error: disk error");
        return false;
    }

    // konsole_printf("%x\n", listdir);

    if (listdir->elements_count != 0)
        konsole_println("========");
    else
    {
        dynamic_array_destroy(listdir);
        return false;
    }

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

        free(file_info->filename);
    }

    dynamic_array_destroy(listdir);

    return true;
}

fat32_basic_file_info_t *terminal_resolve_filename(dynamic_array_t *files)
{
    fat32_basic_file_info_t *result = malloc(sizeof(fat32_basic_file_info_t));
    uint32_t choice = 0;

    if (files->elements_count == 1)
        memcpy(result, dynamic_array_get_top(files), sizeof(fat32_basic_file_info_t));
    else
    {
        konsole_println("Which one?");

        for (uint32_t i = 0;i < files->elements_count;i++)
        {
            fat32_basic_file_info_t *file = dynamic_array_get_by_index(files, i);

            datetime_t dt;
            datetime_datetime_from_fat(&(file->creation_datetime), &dt);

            konsole_printf("[%d]: '%s' (%s), created at %d-%d-%d %d:%d:%d",
                i,
                file->filename,
                file->attributes & FAT32_ATTRIBUTE_DIRECTORY ? "dir" : "file",
                dt.day, dt.month, dt.year, dt.hour, dt.minute, dt.second);

            if (!(file->attributes & FAT32_ATTRIBUTE_DIRECTORY))
                konsole_printf(", size: %d B", file->size);

            konsole_println("");
        }

        konsole_print("?>");

        const char *choice_str = terminal_get_input_line();

        if (choice_str == NULL)
        {
            free(result);
            result = NULL;
        }
        else
        {
            choice = string_to_uint32(choice_str, 10);

            if (choice != -1 && choice < files->elements_count)
                memcpy(result, dynamic_array_get_by_index(files, choice), sizeof(fat32_basic_file_info_t));
            else
            {
                konsole_println("Error: invalid option");
                free(result);
                result = NULL;
            }
        }
    }

    for (uint32_t i = 0;i < files->elements_count;i++)
    {
        if (i == choice)
            continue;
        fat32_basic_file_info_t *file = dynamic_array_get_by_index(files, i);
        free(file->filename);
    }

    dynamic_array_destroy(files);

    return result;
}

bool_t terminal_view(argparse_command_t *command)
{
    uint32_t start_pos = 0;
    uint32_t bytes_count = 0;
    char *pattern = NULL;
    bool_t hex = false;

    for (uint32_t i = 0;i < command->arguments->elements_count;i++)
    {
        argparse_argument_t *arg = dynamic_array_get_by_index(command->arguments, i);

        if (arg->value == NULL)
            pattern = arg->name;
        else if (strcmp(arg->name, "start") == 0)
            start_pos = string_to_uint32(arg->value, 10);
        else if (strcmp(arg->name, "bytes") == 0)
            bytes_count = string_to_uint32(arg->value, 10);
        else if (strcmp(arg->name, "format") == 0)
        {
            if (strcmp(arg->value, "hex") == 0)
                hex = true;
            else if (strcmp(arg->value, "text") == 0)
                hex = false;
            else
            {
                konsole_println("Error: unknown format");
                return false;
            }
        }
    }

    dynamic_array_t *files = fat32_find_files(&info, dynamic_array_get_top(path), pattern, false);

    if (files == NULL)
    {
        konsole_println("Error: disk error");
        return false;
    }

    if (files->elements_count == 0)
    {
        konsole_println("Error: No such file");
        dynamic_array_destroy(files);
        return false;
    }

    fat32_basic_file_info_t *file = terminal_resolve_filename(files);

    if (file == NULL)
    {
        return false;
    }
    if (file->attributes & FAT32_ATTRIBUTE_DIRECTORY)
    {
        free(file);
        konsole_println("Error: it isn't file");
        return false;
    }
    else if (file->size == 0)
    {
        free(file);
        return true;
    }

    uint32_t buffer_size = bytes_count == 0 ? file->size + 1 : min(bytes_count, file->size) + 1;
    unsigned char *buffer = malloc(buffer_size);
    buffer[buffer_size - 1] = '\0';

    fat32_read_file(&info, file, start_pos, buffer, buffer_size - 1);

    if (hex)
    {
        for (uint32_t i = 0;i < buffer_size - 1;i++)
            konsole_printf("%x ", (uint32_t)buffer[i]);
        konsole_println("");
    }
    else
    {
        konsole_println(buffer);
    }

    free(file);
    free(buffer);

    return true;
}

bool_t terminal_change_dir(argparse_command_t *command)
{
    char *pattern = NULL;
    bool_t ignore_case = false;

    for (uint32_t i = 0;i < command->arguments->elements_count;i++)
    {
        argparse_argument_t *arg = dynamic_array_get_by_index(command->arguments, i);

        if (arg->value == 0)
            pattern = arg->name;
        else if (strcmp(arg->name, "opt") == 0)
        {
            if (strcmp(arg->value, "ignore-case") == 0)
                ignore_case = true;
            else
            {
                konsole_printf("Error: invalid option '%s'\n", arg->value);
                return false;
            }
        }
    }

    dynamic_array_t *files = fat32_find_files(&info, dynamic_array_get_top(path), pattern, ignore_case);

    if (files == NULL)
    {
        konsole_println("Error: disk error");
        return false;
    }

    if (files->elements_count == 0)
    {
        konsole_println("Error: No such directory");
        dynamic_array_destroy(files);
        return false;
    }

    fat32_basic_file_info_t *dir = terminal_resolve_filename(files);

    if (dir == NULL)
    {
        return false;
    }
    if (!(dir->attributes & FAT32_ATTRIBUTE_DIRECTORY))
    {
        free(dir);
        konsole_println("Error: it isn't directory");
        return false;
    }

    if (strcmp(dir->filename, ".") == 0)
    {
        free(dir);
        return true;
    }
    else if (strcmp(dir->filename, "..") == 0)
    {
        fat32_basic_file_info_t *current_directory = dynamic_array_get_top(path);
        free(current_directory->filename);
        dynamic_array_pop_back(path);
    }
    else
    {
        dynamic_array_push_back(path, dir);
    }

    free(dir);

    return true;
}

bool_t terminal_write(argparse_command_t *command)
{
    uint32_t start_pos = 0;
    char *pattern = NULL;
    bool_t hex = false;
    bool_t overwrite = false;
    bool_t append = false;
    bool_t ignore_case = false;
    
    for (uint32_t i = 0;i < command->arguments->elements_count;i++)
    {
        argparse_argument_t *arg = dynamic_array_get_by_index(command->arguments, i);

        if (arg->value == NULL)
            pattern = arg->name;
        else if (strcmp(arg->name, "start") == 0)
            start_pos = string_to_uint32(arg->value, 10);
        else if (strcmp(arg->name, "format") == 0)
        {
            if (strcmp(arg->value, "hex") == 0)
                hex = true;
            else if (strcmp(arg->value, "text") == 0)
                hex = false;
            else
            {
                konsole_printf("Error: unknown format '%s'\n", arg->value);
                return false;
            }
        }
        else if (strcmp(arg->name, "opt") == 0)
        {
            if (strcmp(arg->value, "overwrite") == 0)
                overwrite = true;
            else if (strcmp(arg->value, "append") == 0)
                append = true;
            else if (strcmp(arg->value, "ignore-case"))
                ignore_case = true;
            else
            {
                konsole_printf("Error: unknown option '%s'\n", arg->value);
                return false;
            }
        }
    }

    dynamic_array_t *files = fat32_find_files(&info, dynamic_array_get_top(path), pattern, ignore_case);

    if (files == NULL)
    {
        konsole_println("Error: disk error");
        return false;
    }

    if (files->elements_count == 0)
    {
        konsole_println("Error: No such file");
        dynamic_array_destroy(files);
        return false;
    }

    fat32_basic_file_info_t *file = terminal_resolve_filename(files);

    if (file == NULL)
    {
        return false;
    }
    if (file->attributes & FAT32_ATTRIBUTE_DIRECTORY)
    {
        free(file);
        konsole_println("Error: it isn't file");
        return false;
    }

    if (append)
        start_pos = file->size;
    if (overwrite)
    {
        if (!fat32_erase_file_sync(&info, file))
        {
            free(file);
            konsole_println("Error: erase error");
            return false;
        }

        file->size = 0;
        file->cluster_num = 0;
    }
        
    konsole_println("Write text, press Ctrl+C on new line to exit...\n===================");

    const unsigned char *input_line;

    uint32_t buffer_size = 32;
    uint32_t buffer_free_space = buffer_size;
    bool_t enter = false;

    byte_t *buffer = malloc(buffer_size);

    if (hex)
    {
        while (input_line = terminal_get_input_line())
        {
            unsigned char write_byte [3];
            uint8_t cursor = 0;

            for (unsigned char *cur = input_line;;cur++)
            {
                if (*cur == ' ' || *cur == '\0')
                {
                    write_byte[cursor] = '\0';
                    cursor = 0;

                    if (buffer_free_space == 0)
                    {
                        buffer_free_space += buffer_size;
                        buffer_size *= 2;
                        realloc(buffer, buffer_size);
                    }

                    uint32_t byte_num = string_to_uint32(write_byte, 16);

                    if (byte_num != -1)
                    {
                        buffer[buffer_size - buffer_free_space] = byte_num & 0xFF;
                    }
                    else
                    {
                        free(buffer);
                        free(file);
                        konsole_println("Error: invalid byte");
                        return false;
                    }

                    buffer_free_space--;
                }
                else
                    write_byte[cursor++] = *cur;
                
                if (cursor > 3)
                {
                    free(buffer);
                    free(file);
                    konsole_println("Error: invalid byte");
                    return false;
                }

                if (*cur == '\0')
                    break;
            }
        }
    }
    else
    {
        while (input_line = terminal_get_input_line())
        {
            uint32_t length = strlen(input_line);

            while (buffer_free_space < length + enter)
            {
                buffer_free_space += buffer_size;
                buffer_size *= 2;
                buffer = realloc(buffer, buffer_size);
            }

            if (enter)
            {
                buffer[buffer_size - buffer_free_space] = '\n';
                buffer_free_space--;
            }
            enter = true;

            memcpy(buffer + (buffer_size - buffer_free_space), input_line, length);
            buffer_free_space -= length;
        }
    }

    konsole_println("===================");

    if (!fat32_write_file_sync(&info, file, start_pos, buffer, buffer_size - buffer_free_space))
    {
        free(buffer);
        free(file);
        konsole_println("Error: disk write error");
        return false;
    }

    free(buffer);
    free(file);

    konsole_println("Success");

    return true;
}

bool_t terminal_newfile(argparse_command_t *command)
{
    const char *filename = NULL;
    uint8_t attributes = 0;

    for (uint32_t i = 0;i < command->arguments->elements_count;i++)
    {
        argparse_argument_t *arg = dynamic_array_get_by_index(command->arguments, i);

        if (arg->value == NULL)
            filename = arg->name;
        else if (strcmp(arg->name, "attr") == 0)
        {
            if (strcmp(arg->value, "hidden") == 0)
                attributes |= FAT32_ATTRIBUTE_HIDDEN;
            else if (strcmp(arg->value, "archive") == 0)
                attributes |= FAT32_ATTRIBUTE_ARCHIVE;
            else if (strcmp(arg->value, "system") == 0)
                attributes |= FAT32_ATTRIBUTE_SYSTEM;
            else if (strcmp(arg->value, "readonly") == 0)
                attributes |= FAT32_ATTRIBUTE_READONLY;
            else
            {
                konsole_printf("Error: unknown file attribute '%s'\n", arg->value);
                return false;
            }
        }
    }

    if (attributes == 0)
        attributes = FAT32_ATTRIBUTE_ARCHIVE;
    if (filename == NULL || strlen(filename) == 0)
    {
        konsole_println("Error: filename not specified");
        return false;
    }
    
    if (!fat32_create_file(&info, dynamic_array_get_top(path), filename, attributes, NULL))
    {
        konsole_println("Error: filesystem error");
        return false;
    }
    
    return true;
}

bool_t terminal_newdir(argparse_command_t *command)
{
    const char *dirname = NULL;
    uint8_t attributes = 0;

    for (uint32_t i = 0;i < command->arguments->elements_count;i++)
    {
        argparse_argument_t *arg = dynamic_array_get_by_index(command->arguments, i);

        if (arg->value == NULL)
            dirname = arg->name;
        else if (strcmp(arg->name, "attr") == 0)
        {
            if (strcmp(arg->value, "hidden") == 0)
                attributes |= FAT32_ATTRIBUTE_HIDDEN;
            else if (strcmp(arg->value, "system") == 0)
                attributes |= FAT32_ATTRIBUTE_SYSTEM;
            else if (strcmp(arg->value, "readonly") == 0)
                attributes |= FAT32_ATTRIBUTE_READONLY;
            else
            {
                konsole_printf("Error: unknown directory attribute '%s'\n", arg->value);
                return false;
            }
        }
    }

    if (dirname == NULL || strlen(dirname) == 0)
    {
        konsole_println("Error: directory name not specified");
        return false;
    }
    
    if (!fat32_create_directory(&info, dynamic_array_get_top(path), dirname, attributes, NULL))
    {
        konsole_println("Error: filesystem error");
        return false;
    }
    
    return true;
}

bool_t terminal_remove(argparse_command_t *command)
{
    char *pattern = NULL;
    bool_t ignore_case = false;

    for (uint32_t i = 0;i < command->arguments->elements_count;i++)
    {
        argparse_argument_t *arg = dynamic_array_get_by_index(command->arguments, i);

        if (arg->value == 0)
            pattern = arg->name;
        else if (strcmp(arg->name, "opt") == 0)
        {
            if (strcmp(arg->value, "ignore-case") == 0)
                ignore_case = true;
            else
            {
                konsole_printf("Error: invalid option '%s'\n", arg->value);
                return false;
            }
        }
    }

    dynamic_array_t *files = fat32_find_files(&info, dynamic_array_get_top(path), pattern, ignore_case);

    if (files == NULL)
    {
        konsole_println("Error: disk error");
        return false;
    }

    if (files->elements_count == 0)
    {
        konsole_println("Error: no files found");
        dynamic_array_destroy(files);
        return false;
    }

    for (uint32_t i = 0;i < files->elements_count;i++)
    {
        fat32_basic_file_info_t *file = dynamic_array_get_by_index(files, i);

        if (!fat32_remove_file(&info, file))
            konsole_printf("Error: can not remove file '%s'\n", file->filename);
    }

    fat32_destroy_files_list(files);

    return true;
}

// --------- Хэндлер ---------

void terminal_handle_command(const char *buffer)
{
    argparse_command_t command;
    argparse_parse_command(buffer, &command);

    terminal_command_handler_cb *handler = hash_table_get(cmd_handlers, command.command_name, strlen(command.command_name) + 1);
    
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
    terminal_register_command_handler("ls", terminal_print_listdir);
    terminal_register_command_handler("view", terminal_view);
    terminal_register_command_handler("cd", terminal_change_dir);
    terminal_register_command_handler("write", terminal_write);
    terminal_register_command_handler("newfile", terminal_newfile);
    terminal_register_command_handler("newdir", terminal_newdir);
    terminal_register_command_handler("rm", terminal_remove);

    konsole_println("");

    while (1)
    {
        terminal_print_path(NULL);
        konsole_print(">");

        const char *cmd = terminal_get_input_line();

        if (cmd)
            terminal_handle_command(cmd);

        konsole_println("");
    }
}
