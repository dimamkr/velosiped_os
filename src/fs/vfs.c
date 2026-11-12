#include "vfs.h"

static hash_table_t *mountpoints;
static hash_table_t *mountpoints_rev;

static const char* vfs_status_table [] = {
    [VFS_RESULT_OK] = "Ok",
    [VFS_RESULT_INVALID_HANDLE] = "Invalid handle",
    [VFS_RESULT_INVALID_FILENAME] = "Invalid filename",
    [VFS_RESULT_DISK_ERROR] = "Disk error",
    [VFS_RESULT_INVALID_DISK_ID] = "Invalid disk ID",
    [VFS_RESULT_INVALID_PARTITION_NUM] = "Invalid partition num",
    [VFS_RESULT_NO_FILESYSTEM] = "No filesystem",
    [VFS_RESULT_MOUNTPOINT_ALREADY_EXISTS] = "Mountpoint already exists",
    [VFS_RESULT_MOUNTPOINT_NOT_EXISTS] = "Mountpoint not exists",
    [VFS_RESULT_PARTITION_ALREADY_MOUNTED] = "Partition already mounted",
    [VFS_RESULT_INVALID_PARAMETER] = "Invalid parameter",
    [VFS_RESULT_INVALID_METHOD] = "Invalid method",
    [VFS_RESULT_EMPTY_PATH] = "Path is empty",
    [VFS_RESULT_FILESYSTEM_MISMATCH] = "Filesystem mismatch",
    [VFS_RESULT_FILE_ALREADY_EXISTS] = "File already exists",
    [VFS_RESULT_NOT_WRITABLE] = "Stream is not writable",
    [VFS_RESULT_NOT_READABLE] = "Stream is not readable",
    [VFS_RESULT_FILESYSTEM_IS_BUSY] = "Filesystem is busy",
    [VFS_RESULT_FILE_NOT_FOUND] = "File not found"
};

static const vfs_platform_specific_methods_t *vfs_platform_table [256] = {NULL};

const char *vfs_get_result(vfs_result_e result)
{
    return vfs_status_table[result];
}

// объекты файлов должны очищаться этой функцией
void vfs_cleanup_file_obj(vfs_file_t *file)
{
    file->info->methods->release_file_object(file->_file);

    RC_RELEASE(file->info);
}

static void vfs_info_cleanup_routine(void *_info)
{
    vfs_info_t *info = _info;

    info->methods->release_info_object(info->_info);

    free(info);
}

vfs_result_e vfs_copy_file_obj(vfs_file_t *dst, const vfs_file_t *src)
{
    if (dst == NULL || src == NULL)
        return VFS_RESULT_INVALID_HANDLE;

    dst->_file = src->info->methods->copy_file_object(src->_file);

    if (!RC_TAKE(src->info))
    {
        src->info->methods->release_file_object(dst->_file);

        return VFS_RESULT_NO_FILESYSTEM;
    }

    dst->info = src->info;

    vfs_result_e status = dst->info->methods->get_file_parameters(dst->info->_info, dst->_file, &(dst->header));

    if (status != VFS_RESULT_OK)
    {
        dst->info->methods->release_file_object(dst->_file);
        RC_RELEASE(dst->info);
        dst->info = NULL;

        return status;
    }

    return VFS_RESULT_OK;
}

void vfs_destroy_files_array(dynamic_array_t *files)
{
    for (uint32_t i = 0;i < files->elements_count;i++)
    {
        vfs_file_t *file = dynamic_array_get_by_index(files, i);

        vfs_cleanup_file_obj(file);
    }

    dynamic_array_destroy(files);
}

vfs_result_e vfs_mount_partition(uint8_t disk_id, uint8_t partition_num, char *mountpoint)
{
    const vfs_platform_specific_methods_t *methods = vfs_detect_platform(disk_id, partition_num);

    if (methods == NULL)
        return VFS_RESULT_FILESYSTEM_MISMATCH;

    if (hash_table_get(mountpoints, mountpoint, strlen(mountpoint) + 1) != NULL)
        return VFS_RESULT_MOUNTPOINT_ALREADY_EXISTS;

    uint16_t key = MAKEWORD(disk_id, partition_num);

    if (hash_table_get(mountpoints_rev, &key, sizeof(uint16_t)) != NULL)
        return VFS_RESULT_PARTITION_ALREADY_MOUNTED;

    vfs_info_t *info = malloc(sizeof(vfs_info_t));
    vfs_file_t *file = malloc(sizeof(vfs_file_t));

    RC_INIT(info, vfs_info_cleanup_routine);

    info->methods = methods;
    info->_info = methods->allocate_info_object();
    vfs_result_e result = methods->get_info(disk_id, partition_num, info->_info);

    if (result != VFS_RESULT_OK)
    {
        free(info);
        free(file);

        return result;
    }

    result = methods->get_info_header(info->_info, &(info->header));

    if (result != VFS_RESULT_OK)
    {
        free(info);
        free(file);
        methods->release_info_object(info->_info);

        return result;
    }

    file->info = info;
    RC_TAKE(info);

    result = methods->mount(info->_info, mountpoint, &(file->_file));

    if (result != VFS_RESULT_OK)
    {
        methods->release_info_object(info->_info);
        free(file);
        free(info);

        return result;
    }

    result = methods->get_file_parameters(info->_info, file->_file, &(file->header));

    if (result != VFS_RESULT_OK)
    {
        methods->release_file_object(file->_file);
        methods->release_info_object(info->_info);
        free(file);
        free(info);

        return result;
    }

    hash_table_insert(mountpoints, mountpoint, strlen(mountpoint) + 1, file, sizeof(vfs_file_t));
    hash_table_insert(mountpoints_rev, &key, sizeof(uint16_t), mountpoint, strlen(mountpoint) + 1);

    return VFS_RESULT_OK;
}

vfs_result_e vfs_unmount_partition(uint8_t disk_id, uint8_t partition_id, uint32_t threshold)
{
    uint16_t key = MAKEWORD(disk_id, partition_id);

    char *mountpoint = hash_table_get(mountpoints_rev, &key, sizeof(uint16_t));

    if (mountpoint == NULL)
        return VFS_RESULT_MOUNTPOINT_NOT_EXISTS;

    vfs_file_t *mp = hash_table_get(mountpoints, mountpoint, strlen(mountpoint) + 1);

    if (!RC_DESTROY_IF_LESS(mp->info, threshold))
        return VFS_RESULT_FILESYSTEM_IS_BUSY;

    disk_flush_cache_sync(disk_id);
    vfs_cleanup_file_obj(mp);

    hash_table_erase(mountpoints, mountpoint, strlen(mountpoint) + 1);
    hash_table_erase(mountpoints_rev, &key, sizeof(uint16_t));

    return VFS_RESULT_OK;
}

dynamic_array_t *vfs_search_mountpoints(const char *pattern, bool_t ignore_case)
{
    dynamic_array_t *results = dynamic_array_create(sizeof(vfs_file_t));

    for (uint32_t i = 0; i < primes[mountpoints->buff_count_index]; i++)
    {
        for (linked_list_node_t *node = *(mountpoints->key_buff_root + i); node; node = node->right)
        {
            const char *key = node->value;

            if (is_matching_pattern(key, pattern, ignore_case))
            {
                vfs_file_t *mp = hash_table_get(mountpoints, node->value, node->value_size);

                dynamic_array_push_back(results, mp);

                if (vfs_copy_file_obj(dynamic_array_get_top(results), dynamic_array_get_top(results)) != VFS_RESULT_OK)
                    dynamic_array_pop_back(results);
            }
        }
    }

    return results;
}

void vfs_unmount_partitions_by_name(const char *mountpoint, bool_t ignore_case)
{
    dynamic_array_t *partitions = vfs_search_mountpoints(mountpoint, ignore_case);

    for (uint32_t i = 0;i < partitions->elements_count;i++)
    {
        vfs_file_t *mp = dynamic_array_get_by_index(partitions, i);

        vfs_unmount_partition(mp->info->header.disk_id, mp->info->header.partition_num, VFS_DEFAULT_UMOUNT_THRESHOLD + 1);
    }

    vfs_destroy_files_array(partitions);
}

vfs_result_e vfs_open_directory(vfs_file_t *directory, uint32_t iterator_quant_size, const char *pattern, bool_t ignore_case, vfs_directory_reader_t **_result)
{
    if (directory == NULL)
        return VFS_RESULT_INVALID_HANDLE;
    else if (_result == NULL || iterator_quant_size == 0 || !(directory->header.attributes & VFS_ATTRIBUTE_DIRECTORY))
        return VFS_RESULT_INVALID_PARAMETER;

    vfs_directory_reader_t *reader = malloc(sizeof(vfs_directory_reader_t));
    vfs_result_e result = directory->info->methods->open_directory(directory->info->_info, directory->_file, &(reader->_reader), iterator_quant_size);

    if (result != VFS_RESULT_OK)
    {
        free(reader);

        return result;
    }

    reader->directory = malloc(sizeof(vfs_file_t));
    result = vfs_copy_file_obj(reader->directory, directory);

    if (result != VFS_RESULT_OK)
    {
        directory->info->methods->close_directory(reader->_reader);
        free(reader);

        return result;
    }

    if (pattern != NULL)
        reader->pattern = strdup(pattern);
    reader->ignore_case = ignore_case;
    reader->retention_buffer = malloc(iterator_quant_size * sizeof(vfs_file_t)); // unsafe (переполнение)
    reader->retention_buffer_size = iterator_quant_size;
    reader->retention_buffer_index = iterator_quant_size;

    *_result = reader;

    return VFS_RESULT_OK;
}

// эта функция - основа большей функции для рекурсивного поиска файлов
vfs_result_e vfs_read_directory(vfs_directory_reader_t *reader, vfs_file_t *buffer, uint32_t buffer_size, uint32_t *read_size)
{
    if (reader == NULL)
        return VFS_RESULT_INVALID_HANDLE;
    if (buffer == NULL)
        return VFS_RESULT_INVALID_PARAMETER;

    const vfs_platform_specific_methods_t *methods = reader->directory->info->methods;

    uint32_t files_elapsed = buffer_size;

    vfs_file_t tmp_file;
    tmp_file.info = reader->directory->info;

    // обрабатываем остаток файлов от прошлого поиска (retention)

    for (;reader->retention_buffer_index < reader->retention_buffer_size;reader->retention_buffer_index++)
    {
        if (files_elapsed == 0)
            break;

        tmp_file._file = reader->retention_buffer[reader->retention_buffer_index];
            
        if (methods->get_file_parameters(tmp_file.info, tmp_file._file, &(tmp_file.header)) != VFS_RESULT_OK)
        {
            methods->release_file_object(tmp_file._file);

            continue;
        }

        RC_TAKE(tmp_file.info);

        if (reader->pattern == NULL || is_matching_pattern(tmp_file.header.filename, reader->pattern, reader->ignore_case))
        {
            memcpy(&(buffer[buffer_size - files_elapsed]), &tmp_file, sizeof(vfs_file_t));
            files_elapsed--;
        }
        else
            vfs_cleanup_file_obj(&tmp_file);
    }

    *read_size = buffer_size - files_elapsed;

    // обрабатываем основную часть большими кусками

    uint32_t quants_elapsed = files_elapsed / reader->retention_buffer_size + !!(files_elapsed % reader->retention_buffer_size);

    vfs_platform_specific_file_ptr *buffer_ps = malloc(sizeof(vfs_platform_specific_file_ptr) * quants_elapsed * reader->retention_buffer_size); // unsafe
    uint32_t read_size_ps = 0;
    uint32_t buffer_ps_ind = 0;
    
    while (files_elapsed != 0)
    {
        vfs_result_e result = methods->read_directory(reader->_reader, buffer_ps, quants_elapsed * reader->retention_buffer_size, &read_size_ps);

        if (result != VFS_RESULT_OK)
        {
            free(buffer_ps);

            return result;
        }
        if (read_size_ps == 0)
            break;

        for (buffer_ps_ind = 0;buffer_ps_ind < read_size_ps;buffer_ps_ind++)
        {
            if (files_elapsed == 0)
                break;

            tmp_file._file = buffer_ps[buffer_ps_ind];
            
            if (methods->get_file_parameters(tmp_file.info, tmp_file._file, &(tmp_file.header)) != VFS_RESULT_OK)
            {
                methods->release_file_object(tmp_file._file);

                continue;
            }

            RC_TAKE(tmp_file.info);

            if (reader->pattern == NULL || is_matching_pattern(tmp_file.header.filename, reader->pattern, reader->ignore_case))
            {
                memcpy(&(buffer[buffer_size - files_elapsed]), &tmp_file, sizeof(vfs_file_t));
                files_elapsed--;
            }
            else
                vfs_cleanup_file_obj(&tmp_file);
        }

        if (files_elapsed == 0)
            break;

        quants_elapsed = files_elapsed / reader->retention_buffer_size + !!(files_elapsed % reader->retention_buffer_size);
        buffer_ps = realloc(buffer_ps, sizeof(vfs_platform_specific_file_ptr) * quants_elapsed * reader->retention_buffer_size);
    }

    *read_size = buffer_size - files_elapsed;

    // если чтение прочитало все, что можно, то остатка быть не может

    if (files_elapsed > 0)
        reader->retention_buffer_index = reader->retention_buffer_size;
    else
    {
        // остаток кладем в retention
        // (если мы не покрыли весь запрос прошлым остатком)

        uint32_t retention_size = read_size_ps - buffer_ps_ind;

        if (retention_size > 0)
        {
            reader->retention_buffer_index = reader->retention_buffer_size - retention_size;
            memcpy(reader->retention_buffer + reader->retention_buffer_index, buffer_ps + buffer_ps_ind, retention_size * sizeof(vfs_platform_specific_file_ptr));
        }
    }

    free(buffer_ps);

    return VFS_RESULT_OK;
}

void vfs_close_directory(vfs_directory_reader_t *reader)
{
    if (reader == NULL)
        return;

    const vfs_platform_specific_methods_t *methods = reader->directory->info->methods;

    for (uint32_t i = reader->retention_buffer_index;i < reader->retention_buffer_size;i++)
        methods->release_file_object(reader->retention_buffer + i);

    methods->close_directory(reader->_reader);

    vfs_cleanup_file_obj(reader->directory);
    if (reader->pattern != NULL)
        free(reader->pattern);

    free(reader);
}

vfs_result_e vfs_create_tree_searcher(vfs_file_t *directory, const char *pattern, bool_t ignore_case, vfs_tree_searcher_t **_searcher)
{
    if (pattern == NULL)
        pattern = "*";

    vfs_tree_searcher_t *tree = malloc(sizeof(vfs_tree_searcher_t));
    memset(tree, '\0', sizeof(vfs_tree_searcher_t));

    tree->ignore_case = ignore_case;

    // если путь начинается со слеша, нам надо поискать точки монтирования с таким названием
    if (pattern[0] == '/')
    {
        pattern++;
        const uint32_t right_index_num = strchr(pattern, '/');
        char *right_index = (right_index_num == -1) ? NULL : (char*)pattern + right_index_num;
        if (right_index != NULL)
            *right_index = '\0';

        tree->roots = vfs_search_mountpoints(pattern, ignore_case);

        if (right_index != NULL)
            pattern = right_index + 1;
        else
            pattern += strlen(pattern);
    }
    else if (directory == NULL)
    {
        free(tree);

        return VFS_RESULT_INVALID_PARAMETER;
    }
    else
    {
        tree->roots = dynamic_array_create(sizeof(vfs_file_t));
        dynamic_array_push_back(tree->roots, directory);
        
        vfs_result_e result = vfs_copy_file_obj(dynamic_array_get_top(tree->roots), directory);

        if (result != VFS_RESULT_OK)
        {
            vfs_destroy_files_array(tree->roots);
            free(tree);

            return result;
        }
    }

    if (pattern[0] != '\0')
    {
        uint32_t path_depth = 1;

        for (const char *i = pattern;*i;i++)
            if (*i == '/')
                path_depth++;

        tree->subpatterns = malloc(sizeof(char*) * path_depth);
        tree->pattern = strdup(pattern);

        char *last_index = tree->pattern;
        uint32_t subpattern_index = 0;

        for (char *i = last_index;*i;i++)
            if (*i == '/')
            {
                *i = '\0';
                tree->subpatterns[subpattern_index++] = last_index;
                last_index = i + 1;
            }

        tree->subpatterns[subpattern_index] = last_index;

        tree->layers_stack = malloc(sizeof(vfs_directory_reader_t*) * path_depth);
        memset(tree->layers_stack, '\0', sizeof(vfs_directory_reader_t*) * path_depth);
        tree->path_depth = path_depth;
    }

    *_searcher = tree;

    return VFS_RESULT_OK;
}

vfs_result_e vfs_search(vfs_tree_searcher_t *searcher, vfs_file_t *buffer, uint32_t buffer_size, uint32_t *read_size)
{
    if (searcher == NULL)
        return VFS_RESULT_INVALID_HANDLE;

    if (searcher->path_depth == 0)
    {
        uint32_t files_to_copy = min(buffer_size, searcher->roots->elements_count);

        if (files_to_copy > 0)
            dynamic_array_copy_to(searcher->roots, 0, files_to_copy - 1, buffer);
        dynamic_array_pop_front_many(searcher->roots, files_to_copy);

        *read_size = files_to_copy;
    }
    else
    {
        uint32_t level = 0;
        for (;level < searcher->path_depth;level++)
            if (searcher->layers_stack[level] == NULL)
                break;

        uint32_t files_elapsed = buffer_size;

        while (files_elapsed > 0)
        {
            bool_t exit_without_read = false;

            while (level < searcher->path_depth)
            {
                if (level == 0)
                {
                    if (searcher->roots->elements_count == 0)
                    {
                        exit_without_read = true;
                        break;
                    }
                        
                    vfs_file_t *current_dir = dynamic_array_get_top(searcher->roots);
                    vfs_result_e result = vfs_open_directory(current_dir, VFS_ITERATOR_DEFAULT_QUANT_SIZE, searcher->subpatterns[level], searcher->ignore_case, searcher->layers_stack + level);

                    vfs_cleanup_file_obj(current_dir);
                    dynamic_array_pop_front(searcher->roots);

                    if (result != VFS_RESULT_OK)
                        continue;

                    level++;
                }
                else
                {
                    vfs_file_t current_dir;
                    uint32_t read_size;
                    vfs_result_e result = vfs_read_directory(searcher->layers_stack[level-1], &current_dir, 1, &read_size);

                    if (result != VFS_RESULT_OK)
                        continue;

                    if (read_size == 0)
                    {
                        vfs_close_directory(searcher->layers_stack[level-1]);
                        searcher->layers_stack[level-1] = NULL;
                        level--;

                        continue;
                    }

                    result = vfs_open_directory(&current_dir, VFS_ITERATOR_DEFAULT_QUANT_SIZE, searcher->subpatterns[level], searcher->ignore_case, searcher->layers_stack + level);
                    vfs_cleanup_file_obj(&current_dir);

                    if (result != VFS_RESULT_OK)
                        continue;

                    level++;
                }
            }

            if (exit_without_read)
                break;

            uint32_t files_copied;
            vfs_result_e result = vfs_read_directory(searcher->layers_stack[searcher->path_depth-1], buffer + buffer_size - files_elapsed, files_elapsed, &files_copied);

            if (result != VFS_RESULT_OK || files_copied < files_elapsed)
            {
                vfs_close_directory(searcher->layers_stack[searcher->path_depth-1]);
                searcher->layers_stack[searcher->path_depth-1] = NULL;
                level--;
            }

            if (result == VFS_RESULT_OK)
                files_elapsed -= files_copied;
        }

        *read_size = buffer_size - files_elapsed;
    }

    return VFS_RESULT_OK;
}

void vfs_destroy_tree_searcher(vfs_tree_searcher_t *searcher)
{
    if (searcher == NULL)
        return;

    for (uint32_t i = 0;i < searcher->path_depth;i++)
    {
        if (searcher->layers_stack[i] == NULL)
            break;

        vfs_close_directory(searcher->layers_stack[i]);
    }

    vfs_destroy_files_array(searcher->roots);
    free(searcher->layers_stack);
    free(searcher->subpatterns);
    free(searcher->pattern);

    free(searcher);
}

vfs_result_e vfs_get_file_by_path(vfs_file_t *current_dir, const char *path, vfs_file_t **file)
{
    if (current_dir == NULL)
        return VFS_RESULT_INVALID_HANDLE;
    if (path == NULL)
        return VFS_RESULT_INVALID_PARAMETER;

    char *path_buff = strdup(path);
    char *last_sym = path_buff;

    vfs_file_t *dir = malloc(sizeof(vfs_file_t));
    bool_t return_mp = false;

    if (*path_buff == '/')
    {
        uint32_t next_symbol = strchr(path_buff + 1, '/') + 1;

        if (next_symbol == 0)
        {
            return_mp = true;
            next_symbol = strlen(path_buff);
        }

        path_buff[next_symbol] = '\0';
        last_sym = path_buff + next_symbol + 1;

        dynamic_array_t *mp = vfs_search_mountpoints(path_buff + 1, false);

        if (mp->elements_count > 0)
        {
            memcpy(dir, dynamic_array_get_top(mp), sizeof(vfs_file_t));

            dynamic_array_destroy(mp);
        }
        else
        {
            free(dir);

            return VFS_RESULT_FILE_NOT_FOUND;
        }
    }
    else
    {
        vfs_result_e result = vfs_copy_file_obj(dir, current_dir);

        if (result != VFS_RESULT_OK)
        {
            free(dir);
            free(path_buff);

            return result;
        }
    }

    const vfs_platform_specific_methods_t *methods = dir->info->methods;
    
    vfs_file_t new_file;
    bool_t end_cycle = false;
    
    if (!return_mp)
    {
        for (char *i = last_sym;;i++)
        {
            if (*i == '\0')
                end_cycle = true;
            if (*i == '/')
                *i = '\0';

            if (*i == '\0')
            {
                if (!(dir->header.attributes & VFS_ATTRIBUTE_DIRECTORY))
                {
                    vfs_cleanup_file_obj(dir);
                    free(path_buff);

                    return VFS_RESULT_FILE_NOT_FOUND;
                }

                vfs_result_e result = methods->get_file_by_name(dir->info->_info, dir->_file, last_sym, &(new_file._file));

                if (result != VFS_RESULT_OK)
                {
                    vfs_cleanup_file_obj(dir);
                    free(path_buff);

                    return result;
                }

                result = methods->get_file_parameters(dir->info->_info, new_file._file, &(new_file.header));

                if (result != VFS_RESULT_OK)
                {
                    vfs_cleanup_file_obj(dir);
                    methods->release_file_object(new_file._file);
                    free(path_buff);

                    return result;
                }

                new_file.info = dir->info;

                if (!RC_TAKE(dir->info))
                {
                    vfs_cleanup_file_obj(dir);
                    methods->release_file_object(new_file._file);
                    free(path_buff);

                    return VFS_RESULT_NO_FILESYSTEM;
                }

                vfs_cleanup_file_obj(dir);
                memcpy(dir, &new_file, sizeof(vfs_file_t));

                last_sym = i + 1;
            }

            if (end_cycle)
                break;
        }
    }
    
    *file = dir;

    free(path_buff);

    return VFS_RESULT_OK;
}

vfs_result_e vfs_move_file(vfs_file_t *dst_file, vfs_file_t *src_file)
{
    if (dst_file == NULL || src_file == NULL)
        return VFS_RESULT_INVALID_HANDLE;
    if (dst_file->info != src_file->info)
        return VFS_RESULT_INVALID_METHOD;

    const vfs_platform_specific_methods_t *methods = dst_file->info->methods;

    return methods->move_file(dst_file->info->_info, dst_file->_file, src_file->_file);
}

vfs_result_e vfs_open(vfs_file_t *file, uint8_t flags, vfs_stream_t **_stream)
{
    if (file == NULL || file->header.attributes & VFS_ATTRIBUTE_DIRECTORY || file->header.attributes & VFS_ATTRIBUTE_VOLUME_ID)
        return VFS_RESULT_INVALID_HANDLE;

    vfs_stream_t *stream = malloc(sizeof(vfs_stream_t));
    vfs_result_e result = file->info->methods->open_file(file->info->_info, file->_file, flags, &(stream->_stream));

    if (result != VFS_RESULT_OK)
    {
        free(stream);

        return result;
    }

    stream->flags = flags;
    stream->file = malloc(sizeof(vfs_file_t));
    result = vfs_copy_file_obj(stream->file, file);

    if (result != VFS_RESULT_OK)
    {
        free(stream->file);
        file->info->methods->close_file(stream->_stream);
        free(stream);

        return result;
    }

    *_stream = stream;

    return result;
}

bool_t vfs_is_filename_allowed(const char *filename)
{
    if (*filename == '\0')
        return false;

    for (const char *cur = filename;*cur != '\0';cur++)
    {
        switch (*cur)
        {
            case '?':
            case '*':
            case '/':
                return false;
        }
    }

    return true;
}

vfs_result_e vfs_erase(vfs_file_t *file)
{
    vfs_result_e result = file->info->methods->erase_file(file->info->_info, file->_file);

    if (result != VFS_RESULT_OK)
        return result;

    return file->info->methods->get_file_parameters(file->info->_info, file->_file, &(file->header));
}

vfs_result_e vfs_newfile(vfs_file_t *dir, const char *filename, uint32_t attributes, vfs_file_t **_file)
{
    if (dir == NULL || filename == NULL)
        return VFS_RESULT_INVALID_PARAMETER;
    if (!vfs_is_filename_allowed(filename))
        return VFS_RESULT_INVALID_FILENAME;

    // не проверяем запрещенные комбинации аттрибутов,
    // так как по идее это должен проверить внутренний блок

    vfs_file_t *file = malloc(sizeof(vfs_file_t));
    file->info = dir->info;

    if (!RC_TAKE(file->info))
    {
        dir->info->methods->release_file_object(file->_file);
        free(file);

        return VFS_RESULT_NO_FILESYSTEM;
    }

    vfs_result_e result = dir->info->methods->create_file(dir->info->_info, dir->_file, filename, attributes, &(file->_file));

    if (result != VFS_RESULT_OK)
    {
        RC_RELEASE(file->info);
        free(file);

        return result;
    }

    result = file->info->methods->get_file_parameters(file->info->_info, file->_file, &(file->header));

    if (result != VFS_RESULT_OK)
    {
        vfs_cleanup_file_obj(file);
        free(file);

        return result;
    }

    if (_file)
        *_file = file;

    return VFS_RESULT_OK;
}

vfs_result_e vfs_newdir(vfs_file_t *dir, const char *filename, uint32_t attributes, vfs_file_t **_new_dir)
{
    if (dir == NULL)
        return VFS_RESULT_INVALID_HANDLE;
    if (filename == NULL)
        return VFS_RESULT_INVALID_PARAMETER;
    if (!vfs_is_filename_allowed(filename))
        return VFS_RESULT_INVALID_FILENAME;

    vfs_file_t *new_dir = malloc(sizeof(vfs_file_t));
    new_dir->info = dir->info;

    if (!RC_TAKE(new_dir->info))
    {
        dir->info->methods->release_file_object(new_dir->_file);
        free(new_dir);

        return VFS_RESULT_NO_FILESYSTEM;
    }

    vfs_result_e result = dir->info->methods->create_directory(dir->info->_info, dir->_file, filename, attributes, &(new_dir->_file));

    if (result != VFS_RESULT_OK)
    {
        RC_RELEASE(new_dir->info);
        free(new_dir);

        return result;
    }

    result = new_dir->info->methods->get_file_parameters(new_dir->info->_info, new_dir->_file, &(new_dir->header));

    if (result != VFS_RESULT_OK)
    {
        vfs_cleanup_file_obj(new_dir);
        free(new_dir);

        return result;
    }

    if (_new_dir)
        *_new_dir = new_dir;

    return VFS_RESULT_OK;
}

vfs_result_e vfs_remove(vfs_file_t *file)
{
    if (file == NULL)
        return VFS_RESULT_INVALID_HANDLE;

    return file->info->methods->remove_file(file->info->_info, file->_file);
}

uint32_t vfs_tell(vfs_stream_t *stream)
{
    if (stream == NULL)
        return VFS_RESULT_INVALID_HANDLE;
    
    return stream->file->info->methods->tell(stream->_stream);
}

vfs_result_e vfs_seek(vfs_stream_t *stream, int offset, uint8_t mode) // unsafe (offset int, но tell - unsigned int)
{
    if (stream == NULL)
        return VFS_RESULT_INVALID_HANDLE;

    return stream->file->info->methods->seek(stream->_stream, offset, mode);
}

vfs_result_e vfs_read(vfs_stream_t *stream, void *buffer, uint32_t bytes, uint32_t *bytes_read)
{
    return stream->file->info->methods->read_file(stream->_stream, buffer, bytes, bytes_read);
}

vfs_result_e vfs_write(vfs_stream_t *stream, void *buffer, uint32_t bytes, uint32_t *bytes_wrote)
{
    return stream->file->info->methods->write_file(stream->_stream, buffer, bytes, bytes_wrote);
}

void vfs_close(vfs_stream_t *stream)
{
    disk_flush_cache_sync(stream->file->info->header.disk_id);

    stream->file->info->methods->close_file(stream->_stream);

    vfs_cleanup_file_obj(stream->file);
    free(stream->file);

    free(stream);
}

const vfs_platform_specific_methods_t *vfs_detect_platform(uint8_t disk_id, uint8_t partition_num)
{
    if (disk_id == 0xFF)
        return vfs_platform_table[0xFF]; // специальный случай для ram_fs
    if (partition_num >= 4)
        return NULL;

    dynamic_array_t *disks = disk_enumerate_disks();
    bool_t found = false;

    for (uint32_t i = 0;i < disks->elements_count;i++)
    {
        ata_basic_identify_data_t *data = dynamic_array_get_by_index(disks, i);

        if (data->port_num == disk_id)
        {
            found = true;
            break;
        }
    }

    dynamic_array_destroy(disks);

    if (!found)
        return NULL;

    mbr_t mbr;
    mbr_read_sync(disk_id, &mbr);

    if (mbr.partitions[partition_num].partition_type == MBR_PARTITION_UNUSED)
        return NULL;
    
    return vfs_platform_table[mbr.partitions[partition_num].partition_type];
}

void vfs_register_platform(uint8_t type, const vfs_platform_specific_methods_t *methods)
{
    vfs_platform_table[type] = methods;
}

void vfs_init()
{
    mountpoints = hash_table_create();
    mountpoints_rev = hash_table_create();
}