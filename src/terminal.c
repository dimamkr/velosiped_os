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
#include "task.h"
#include "argparse.h"
#include "hash_table.h"
#include "elf.h"
#include "power.h"
#include "sysenter.h"
#include "vmm.h"
#include "vfs.h"

static char terminal_input_buff[256];
static int terminal_input_buff_lenght;
static bool_t terminal_EOI_flag;
static bool_t terminal_cancelled_flag;

dynamic_array_t *command_history; // хранит указатели на начала строк (char*)
int command_history_index;

vfs_info_t info;
vfs_file_t *root;
vfs_file_t *current_dir;

hash_table_t *cmd_handlers;

static inline void terminal_input_buff_add_symbol(char symbol)
{
    terminal_input_buff[terminal_input_buff_lenght] = symbol;
    terminal_input_buff_lenght++;
}

static inline void terminal_command_erase_last()
{
    if (terminal_input_buff_lenght > 0)
    {
        konsole_putch('\b');
        terminal_input_buff[--terminal_input_buff_lenght] = 0;
    }
}

static inline void terminal_command_clear()
{
    while (terminal_input_buff_lenght)
    {
        terminal_command_erase_last();
    }
}

static inline void terminal_input_buff_reload_from_command_history()
{
    if (command_history_index == command_history->elements_count)
    {
        terminal_command_clear();
        return;
    }

    if (command_history_index < 0)
        return;

    char *command = *(char **)dynamic_array_get_by_index(command_history, command_history_index);

    terminal_command_clear();
    for (uint32_t i = 0; i < strlen(command); ++i)
    {
        terminal_input_buff_add_symbol(command[i]);
        konsole_putch(command[i]);
    }
}

static inline void terminal_command_history_add()
{
    char *str = malloc(terminal_input_buff_lenght + 1);
    memcpy(str, terminal_input_buff, terminal_input_buff_lenght);
    str[terminal_input_buff_lenght] = 0; // чтобы было понятно где конец строки

    dynamic_array_push_back(command_history, &str);

    command_history_index = command_history->elements_count;
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
                terminal_input_buff_add_symbol(0);
                terminal_cancelled_flag = true;
                continue;
            }
        }

        // Обработка специальных клавиш
        switch (ev.keycode)
        {
        case KEY_BACKSPACE:
            terminal_command_erase_last();
            break;
        case KEY_ENTER:
            terminal_input_buff_add_symbol(0);
            terminal_EOI_flag = true;
            break;
        case KEY_SPACE:
            terminal_input_buff_add_symbol(' ');
            konsole_putch(' ');
            break;
        case KEY_UP:
            command_history_index = max2(command_history_index - 1, 0);
            terminal_input_buff_reload_from_command_history();
            break;
        case KEY_DOWN:
            command_history_index = min2(command_history_index + 1, command_history->elements_count);
            terminal_input_buff_reload_from_command_history();
            break;
        default:
            if (ev.keycode < 0x80)
            { // ASCII
                char ch = (char)ev.keycode;
                terminal_input_buff_add_symbol(ch);
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
        terminal_command_history_add();
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

    for (uint32_t i = 0; i < primes[cmd_handlers->buff_count_index]; i++)
    {
        for (linked_list_node_t *node = *(cmd_handlers->key_buff_root + i); node; node = node->right)
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
    dynamic_array_t *disks_info = disk_enumerate_disks();

    if (disks_info->elements_count)
    {
        for (uint8_t i = 0; i < disks_info->elements_count; i++)
        {
            ata_basic_identify_data_t *entry = dynamic_array_get_by_index(disks_info, i);

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
    if (!current_dir)
        return false;

    konsole_printf("%s/", current_dir->filename);

    return true;
}

bool_t terminal_print_listdir(argparse_command_t *command)
{
    if (!current_dir)
    {
        konsole_println("Error: path is empty (ROOT is not mounted)");
        return false;
    }

    dynamic_array_t *listdir;
    char *pattern = NULL;
    bool_t ignore_case = false;

    for (uint32_t i = 0; i < command->arguments->elements_count; i++)
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

    listdir = vfs_find_files(current_dir, pattern, ignore_case);

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

    for (uint32_t i = 0; i < listdir->elements_count; i++)
    {
        vfs_file_t *file_info = dynamic_array_get_by_index(listdir, i);

        konsole_printf("Filename: %s\n", file_info->filename);

        konsole_print("Attributes: ");

        if (file_info->attributes & VFS_ATTRIBUTE_ARCHIVE)
            konsole_print("Archived ");
        if (file_info->attributes & VFS_ATTRIBUTE_DIRECTORY)
            konsole_print("Directory ");
        if (file_info->attributes & VFS_ATTRIBUTE_HIDDEN)
            konsole_print("Hidden ");
        if (file_info->attributes & VFS_ATTRIBUTE_READONLY)
            konsole_print("Readonly ");
        if (file_info->attributes & VFS_ATTRIBUTE_SYSTEM)
            konsole_print("System ");
        if (file_info->attributes & VFS_ATTRIBUTE_VOLUME_ID)
            konsole_print("VolumeID ");

        konsole_println("");

        konsole_printf("Created at: %d-%d-%d %d:%d:%d\n",
            file_info->creation_datetime.day,
            file_info->creation_datetime.month,
            file_info->creation_datetime.year,
            file_info->creation_datetime.hour,
            file_info->creation_datetime.minute,
            file_info->creation_datetime.second);

        konsole_printf("Modified at: %d-%d-%d %d:%d:%d\n",
            file_info->modify_datetime.day,
            file_info->modify_datetime.month,
            file_info->modify_datetime.year,
            file_info->modify_datetime.hour,
            file_info->modify_datetime.minute,
            file_info->modify_datetime.second);

        if (!(file_info->attributes & (VFS_ATTRIBUTE_DIRECTORY | VFS_ATTRIBUTE_VOLUME_ID)))
            konsole_printf("Size: %d B\n", file_info->size);

        konsole_println("========");
    }

    vfs_destroy_files_array(listdir);

    return true;
}

vfs_file_t *terminal_resolve_filename(dynamic_array_t *files)
{
    if (!current_dir)
    {
        konsole_println("Error: path is empty (ROOT is not mounted)");
        return false;
    }

    vfs_file_t *result = malloc(sizeof(vfs_file_t));
    uint32_t choice = 0;

    if (files->elements_count == 1)
        memcpy(result, dynamic_array_get_top(files), sizeof(vfs_file_t));
    else
    {
        konsole_println("Which one?");

        for (uint32_t i = 0; i < files->elements_count; i++)
        {
            vfs_file_t *file = dynamic_array_get_by_index(files, i);

            konsole_printf("[%d]: '%s' (%s), created at %d-%d-%d %d:%d:%d",
                           i,
                           file->filename,
                           file->attributes & VFS_ATTRIBUTE_DIRECTORY ? "dir" : "file",
                           file->creation_datetime.day,
                           file->creation_datetime.month,
                           file->creation_datetime.year,
                           file->creation_datetime.hour,
                           file->creation_datetime.minute,
                           file->creation_datetime.second);

            if (!(file->attributes & VFS_ATTRIBUTE_DIRECTORY))
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
            choice = strtoul(choice_str, NULL, 10);

            if (choice != -1 && choice < files->elements_count)
                memcpy(result, dynamic_array_get_by_index(files, choice), sizeof(vfs_file_t));
            else
            {
                konsole_println("Error: invalid option");
                free(result);
                result = NULL;
            }
        }
    }

    for (uint32_t i = 0; i < files->elements_count; i++)
    {
        if (i == choice)
            continue;
        vfs_file_t *file = dynamic_array_get_by_index(files, i);
        vfs_cleanup_file_obj(file);
    }

    dynamic_array_destroy(files);

    return result;
}

bool_t terminal_view(argparse_command_t *command)
{
    if (!current_dir)
    {
        konsole_println("Error: path is empty (ROOT is not mounted)");
        return false;
    }

    uint32_t start_pos = 0;
    uint32_t bytes_count = 0;
    char *pattern = NULL;
    bool_t hex = false;

    for (uint32_t i = 0; i < command->arguments->elements_count; i++)
    {
        argparse_argument_t *arg = dynamic_array_get_by_index(command->arguments, i);

        if (arg->value == NULL)
            pattern = arg->name;
        else if (strcmp(arg->name, "start") == 0)
            start_pos = strtoul(arg->value, NULL, 10);
        else if (strcmp(arg->name, "bytes") == 0)
            bytes_count = strtoul(arg->value, NULL, 10);
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

    dynamic_array_t *files = vfs_find_files(current_dir, pattern, false);

    if (files == NULL)
    {
        konsole_println("Error: filesystem error");
        return false;
    }

    if (files->elements_count == 0)
    {
        konsole_println("Error: No such file");
        dynamic_array_destroy(files);
        return false;
    }

    vfs_file_t *file = terminal_resolve_filename(files);

    if (file == NULL)
    {
        return false;
    }
    if (file->attributes & VFS_ATTRIBUTE_DIRECTORY)
    {
        vfs_cleanup_file_obj(file);
        free(file);
        konsole_println("Error: it isn't file");
        return false;
    }
    else if (file->size == 0)
    {
        vfs_cleanup_file_obj(file);
        free(file);
        return true;
    }

    uint32_t buffer_size = bytes_count == 0 ? file->size + 1 : min(bytes_count, file->size) + 1;
    unsigned char *buffer = malloc(buffer_size);
    buffer[buffer_size - 1] = '\0';

    vfs_stream_t *stream = vfs_open(file, VFS_FLAG_READ);

    if (start_pos > 0)
        vfs_seek(stream, start_pos, VFS_SEEK_MODE_START);

    if (stream == NULL)
    {
        vfs_cleanup_file_obj(file);
        free(file);
        konsole_println("Error: filesystem error");
        return false;
    }

    vfs_read(stream, buffer, buffer_size - 1);

    if (hex)
    {
        for (uint32_t i = 0; i < buffer_size - 1; i++)
            konsole_printf("%x ", (uint32_t)buffer[i]);
        konsole_println("");
    }
    else
    {
        konsole_println(buffer);
    }

    vfs_close(stream);
    vfs_cleanup_file_obj(file);
    free(file);
    free(buffer);

    return true;
}

bool_t terminal_change_dir(argparse_command_t *command)
{
    if (!current_dir)
    {
        konsole_println("Error: path is empty (ROOT is not mounted)");
        return false;
    }

    char *pattern = NULL;
    bool_t ignore_case = false;

    for (uint32_t i = 0; i < command->arguments->elements_count; i++)
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

    if (pattern == NULL || strlen(pattern) == 0)
    {
        vfs_cleanup_file_obj(current_dir);
        vfs_copy_file_obj(current_dir, root);

        return true;
    }

    dynamic_array_t *files = vfs_find_files(current_dir, pattern, ignore_case);

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

    vfs_file_t *dir = terminal_resolve_filename(files);

    if (dir == NULL)
    {
        return false;
    }
    if (!(dir->attributes & VFS_ATTRIBUTE_DIRECTORY))
    {
        free(dir);
        konsole_println("Error: it isn't directory");
        return false;
    }

    vfs_cleanup_file_obj(current_dir);
    vfs_copy_file_obj(current_dir, dir);

    vfs_cleanup_file_obj(dir);
    free(dir);

    return true;
}

bool_t terminal_write(argparse_command_t *command)
{
    if (!current_dir)
    {
        konsole_println("Error: path is empty (ROOT is not mounted)");
        return false;
    }

    uint32_t start_pos = 0;
    char *pattern = NULL;
    bool_t hex = false;
    uint8_t flags = VFS_FLAG_WRITE;
    bool_t ignore_case = false;

    for (uint32_t i = 0; i < command->arguments->elements_count; i++)
    {
        argparse_argument_t *arg = dynamic_array_get_by_index(command->arguments, i);

        if (arg->value == NULL)
            pattern = arg->name;
        else if (strcmp(arg->name, "start") == 0)
            start_pos = strtoul(arg->value, NULL, 10);
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
                flags |= VFS_FLAG_OVERWRITE;
            else if (strcmp(arg->value, "append") == 0)
                flags |= VFS_FLAG_APPEND;
            else if (strcmp(arg->value, "ignore-case"))
                ignore_case = true;
            else
            {
                konsole_printf("Error: unknown option '%s'\n", arg->value);
                return false;
            }
        }
    }

    dynamic_array_t *files = vfs_find_files(current_dir, pattern, ignore_case);

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

    vfs_file_t *file = terminal_resolve_filename(files);

    if (file == NULL)
    {
        return false;
    }
    if (file->attributes & VFS_ATTRIBUTE_DIRECTORY)
    {
        free(file);
        konsole_println("Error: it isn't file");
        return false;
    }

    konsole_println("Write text, press Ctrl+C on new line to exit...\n===================");

    vfs_stream_t *stream = vfs_open(file, flags);

    if (stream == NULL)
    {
        vfs_cleanup_file_obj(file);
        free(file);
        konsole_println("Error: filesystem error");
        return false;
    }

    if (start_pos > 0)
        vfs_seek(stream, start_pos, VFS_SEEK_MODE_START);

    const unsigned char *input_line;

    if (hex)
    {
        byte_t *buffer = NULL;

        while (input_line = terminal_get_input_line())
        {
            buffer = realloc(buffer, strlen(input_line));
            uint32_t buffer_ind = 0;

            uint8_t write_byte[3];
            uint8_t cursor = 0;

            for (const unsigned char *cur = input_line;; cur++)
            {
                if (*cur == ' ' || *cur == '\0')
                {
                    write_byte[cursor] = '\0';
                    cursor = 0;

                    uint32_t byte_num = strtoul(write_byte, NULL, 16);

                    if (byte_num != -1)
                    {
                        buffer[buffer_ind++] = byte_num & 0xFF;
                    }
                    else
                    {
                        free(buffer);
                        vfs_close(stream);
                        vfs_cleanup_file_obj(file);
                        free(file);
                        konsole_println("Error: invalid byte");
                        return false;
                    }
                }
                else
                    write_byte[cursor++] = *cur;

                if (cursor > 3)
                {
                    free(buffer);
                    vfs_close(stream);
                    vfs_cleanup_file_obj(file);
                    free(file);
                    konsole_println("Error: invalid byte");
                    return false;
                }

                if (*cur == '\0')
                    break;
            }

            vfs_write(stream, buffer, buffer_ind);
        }

        free(buffer);
    }
    else
    {
        while (input_line = terminal_get_input_line())
        {
            vfs_write(stream, (void*)input_line, strlen(input_line));
            vfs_write(stream, (void*)"\n", 1);
        }
    }

    konsole_println("===================");

    vfs_close(stream);
    vfs_cleanup_file_obj(file);
    free(file);

    konsole_println("Success");

    return true;
}

bool_t terminal_newfile(argparse_command_t *command)
{
    if (!current_dir)
    {
        konsole_println("Error: path is empty (ROOT is not mounted)");
        return false;
    }

    const char *filename = NULL;
    uint8_t attributes = 0;

    for (uint32_t i = 0; i < command->arguments->elements_count; i++)
    {
        argparse_argument_t *arg = dynamic_array_get_by_index(command->arguments, i);

        if (arg->value == NULL)
            filename = arg->name;
        else if (strcmp(arg->name, "attr") == 0)
        {
            if (strcmp(arg->value, "hidden") == 0)
                attributes |= VFS_ATTRIBUTE_HIDDEN;
            else if (strcmp(arg->value, "archive") == 0)
                attributes |= VFS_ATTRIBUTE_ARCHIVE;
            else if (strcmp(arg->value, "system") == 0)
                attributes |= VFS_ATTRIBUTE_SYSTEM;
            else if (strcmp(arg->value, "readonly") == 0)
                attributes |= VFS_ATTRIBUTE_READONLY;
            else
            {
                konsole_printf("Error: unknown file attribute '%s'\n", arg->value);
                return false;
            }
        }
    }

    if (attributes == 0)
        attributes = VFS_ATTRIBUTE_ARCHIVE;
    if (filename == NULL || strlen(filename) == 0)
    {
        konsole_println("Error: filename not specified");
        return false;
    }

    if (!vfs_newfile(current_dir, filename, attributes, NULL))
    {
        konsole_println("Error: filesystem error");
        return false;
    }

    return true;
}

bool_t terminal_newdir(argparse_command_t *command)
{
    if (!current_dir)
    {
        konsole_println("Error: path is empty (ROOT is not mounted)");
        return false;
    }

    const char *dirname = NULL;
    uint8_t attributes = 0;

    for (uint32_t i = 0; i < command->arguments->elements_count; i++)
    {
        argparse_argument_t *arg = dynamic_array_get_by_index(command->arguments, i);

        if (arg->value == NULL)
            dirname = arg->name;
        else if (strcmp(arg->name, "attr") == 0)
        {
            if (strcmp(arg->value, "hidden") == 0)
                attributes |= VFS_ATTRIBUTE_HIDDEN;
            else if (strcmp(arg->value, "system") == 0)
                attributes |= VFS_ATTRIBUTE_SYSTEM;
            else if (strcmp(arg->value, "readonly") == 0)
                attributes |= VFS_ATTRIBUTE_READONLY;
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

    if (!vfs_newdir(current_dir, dirname, attributes, NULL))
    {
        konsole_println("Error: filesystem error");
        return false;
    }

    return true;
}

bool_t terminal_remove(argparse_command_t *command)
{
    if (!current_dir)
    {
        konsole_println("Error: path is empty (ROOT is not mounted)");
        return false;
    }

    char *pattern = NULL;
    bool_t ignore_case = false;

    for (uint32_t i = 0; i < command->arguments->elements_count; i++)
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

    dynamic_array_t *files = vfs_find_files(current_dir, pattern, ignore_case);

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

    for (uint32_t i = 0; i < files->elements_count; i++)
    {
        vfs_file_t *file = dynamic_array_get_by_index(files, i);

        if (!vfs_remove(file))
            konsole_printf("Error: can not remove file '%s'\n", file->filename);
    }

    vfs_destroy_files_array(files);

    return true;
}

void foo(void *arg)
{
    konsole_println(arg);
}

bool_t terminal_exec(argparse_command_t *command)
{
    if (!current_dir)
    {
        konsole_println("Error: path is empty (ROOT is not mounted)");
        return false;
    }

    char *file_name = NULL;
    AUTOFREE_PTR(char)
    process_arg = malloc(3);
    memcpy(process_arg, ";(", 3);
    uint32_t stack_size = STACK_SIZE_SMALL;

    for (uint32_t i = 0; i < command->arguments->elements_count; i++)
    {
        argparse_argument_t *arg = dynamic_array_get_by_index(command->arguments, i);

        if (arg->value == NULL && i == 0)
            file_name = arg->name;
        else if (arg->value == NULL && i == 1)
        {
            free(process_arg);
            process_arg = malloc(strlen(arg->name) + 1);
            memcpy(process_arg, arg->name, strlen(arg->name) + 1);
        }
        else if (strcmp(arg->name, "s") == 0)
        {
            stack_size = strtoul(arg->value, NULL, 10);
        }
    }

    dynamic_array_t *files = vfs_find_files(current_dir, file_name, false);

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

    AUTOFREE_PTR(vfs_file_t) file = terminal_resolve_filename(files);

    if (file == NULL)
    {
        return false;
    }
    if (file->attributes & VFS_ATTRIBUTE_DIRECTORY)
    {
        konsole_println("Error: it isn't file");
        return false;
    }

    AUTOFREE_PTR(void)
    buff = malloc(file->size + 1);

    vfs_stream_t *stream = vfs_open(file, VFS_FLAG_READ);
    vfs_read(stream, buff, file->size);
    vfs_close(stream);

    // elf_print_info(buff);

    int argc = 2;
    char *arg1 = "ECHO: ";
    char *arg2 = process_arg;
    char *argv[2] = {arg1, arg2};

    if (!task_create_user_process_from_elf(buff, argc, argv, STACK_SIZE_SMALL, stack_size))
    {
        konsole_println("Error: it isn't elf file");
        return false;
    }

    // task_yield();

    return true;
}

bool_t terminal_poweroff(argparse_command_t *command)
{
    konsole_println("The system will power-off now.");

    return power_gracefully_shutdown();
}

bool_t terminal_reboot(argparse_command_t *command)
{
    konsole_println("The system will reboot now.");

    return power_gracefully_reboot();
}

bool_t terminal_dbg(argparse_command_t *command)
{
    konsole_println("DEBUG COMMAND");

    return true;
}

// --------- Хэндлер ---------

bool_t terminal_mount_root()
{
    uint8_t boot_disk_id = disk_get_boot_disk_id();
    if (boot_disk_id == -1)
        return false;

    uint8_t bootable_partition_id = mbr_get_bootable_partition(boot_disk_id);
    if (bootable_partition_id == -1)
        return false;

    root = vfs_mount_partition(boot_disk_id, bootable_partition_id, "ROOT");
    if (root == NULL)
        return false;

    current_dir = malloc(sizeof(vfs_file_t));
    vfs_copy_file_obj(current_dir, root);

    return true;
}

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

void terminal_init()
{
    if (!terminal_mount_root())
    {
        konsole_set_bad_result_color();
        konsole_println("\nError: can't mount root");
        konsole_set_base_color();
    }

    cmd_handlers = hash_table_create();

    command_history = dynamic_array_create(sizeof(char *));
    command_history_index = 0;

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
    terminal_register_command_handler("exec", terminal_exec);
    terminal_register_command_handler("poweroff", terminal_poweroff);
    terminal_register_command_handler("reboot", terminal_reboot);
    terminal_register_command_handler("dbg", terminal_dbg);
}

void terminal_main_loop()
{
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