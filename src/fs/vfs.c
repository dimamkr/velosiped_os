#include "vfs.h"


static hash_table_t *mountpoints;
static hash_table_t *mountpoints_rev;


static inline uint32_t vfs_convert_attributes_from_fat32(uint8_t fat_attributes)
{
    return (uint32_t)fat_attributes; // наши аттрибуты совместимы с fat32
}

// объекты файлов должны создаваться такими функциями (не руками)
// или нужно учитывать reference counter самому
static inline bool_t vfs_fill_file_obj_from_fat32(vfs_file_t *file)
{
    file->filename = file->fat32.filename;
    file->size = file->fat32.size;
    datetime_datetime_from_fat(&(file->fat32.creation_datetime), &(file->creation_datetime));
    datetime_datetime_from_fat(&(file->fat32.last_modify_datetime), &(file->modify_datetime));
    file->attributes = vfs_convert_attributes_from_fat32(file->fat32.attributes);

    if (!RC_TAKE(file->info))
        return false;

    return true;
}

bool_t vfs_update_file_obj(vfs_file_t *file)
{
    switch (file->info->filesystem)
    {
        case VFS_FS_FAT32:
        {
            file->size = file->fat32.size;
            datetime_datetime_from_fat(&(file->fat32.creation_datetime), &(file->creation_datetime));
            datetime_datetime_from_fat(&(file->fat32.last_modify_datetime), &(file->modify_datetime));
            file->attributes = vfs_convert_attributes_from_fat32(file->fat32.attributes);
            file->filename = file->fat32.filename;

            break;
        }
    }

    return true;
}

// объекты файлов должны очищаться этой функцией
void vfs_cleanup_file_obj(vfs_file_t *file)
{
    free(file->filename);
    RC_RELEASE(file->info);
}

static void vfs_info_cleanup_routine(void *_info)
{
    free(_info);
}

bool_t vfs_copy_file_obj(vfs_file_t *dst, const vfs_file_t *src)
{
    switch (src->info->filesystem)
    {
        case VFS_FS_FAT32:
        {
            memcpy(dst, src, sizeof(vfs_file_t));
            dst->filename = dst->fat32.filename = strdup(src->filename);
        }
    }

    if (!RC_TAKE(src->info))
    {
        if (dst != NULL)
            free(dst->filename);

        return false;
    }

    return true;
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


vfs_file_t *vfs_mount_partition(uint8_t disk_id, uint8_t partition_id, char *mountpoint)
{
    uint16_t key = MAKEWORD(disk_id, partition_id);

    if (partition_id > 4 || \
        hash_table_get(mountpoints, mountpoint, strlen(mountpoint) + 1) != NULL || \
        hash_table_get(mountpoints_rev, &key, sizeof(uint16_t)) != NULL)
        return NULL;

    mbr_t mbr_info;

    if (!mbr_read_sync(disk_id, &mbr_info))
        return NULL;

    vfs_info_t *info = NULL;
    vfs_file_t *result = NULL;

    switch (mbr_info.partitions[partition_id].partition_type)
    {
        case MBR_PARTITION_FAT32_LBA:
        {
            info = malloc(sizeof(vfs_info_t));
            result = malloc(sizeof(vfs_file_t));

            RC_INIT(info, vfs_info_cleanup_routine);

            info->filesystem = VFS_FS_FAT32;
            info->disk_id = disk_id;
            info->partition_id = partition_id;

            info->fat32.disk_id = disk_id;
            info->fat32.partition_start = mbr_info.partitions[partition_id].start_lba;

            fat32_get_info_sync(&(info->fat32));
            fat32_mount(&(info->fat32), mountpoint, &(result->fat32));

            result->info = info;
            vfs_fill_file_obj_from_fat32(result);

            hash_table_insert(mountpoints, mountpoint, strlen(mountpoint) + 1, result, sizeof(vfs_file_t));
            hash_table_insert(mountpoints_rev, &key, sizeof(uint16_t), mountpoint, strlen(mountpoint) + 1);

            break;
        }
    }

    return result;
}


bool_t vfs_unmount_partition(uint8_t disk_id, uint8_t partition_id)
{
    uint16_t key = MAKEWORD(disk_id, partition_id);

    char *mountpoint = hash_table_get(mountpoints_rev, &key, sizeof(uint16_t));

    if (mountpoint == NULL)
        return false;

    vfs_file_t *mp = hash_table_get(mountpoints, mountpoint, strlen(mountpoint) + 1);

    disk_flush_cache_sync(disk_id);

    vfs_cleanup_file_obj(mp);

    hash_table_erase(mountpoints, mountpoint, strlen(mountpoint) + 1);
    hash_table_erase(mountpoints_rev, &key, sizeof(uint16_t));

    return true;
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
                vfs_copy_file_obj(dynamic_array_get_top(results), dynamic_array_get_top(results));
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

        vfs_unmount_partition(mp->info->disk_id, mp->info->partition_id);
    }

    dynamic_array_destroy(partitions);
}

// TODO: сделать штуку типа FindFirstFile, FindNextFile вместо этого
dynamic_array_t *vfs_find_files_in_directory(vfs_file_t *directory, const char *_pattern, bool_t ignore_case)
{
    if (_pattern == NULL)
        _pattern = "*";

    char *pattern = strdup(_pattern);
    uint32_t pattern_length = strlen(pattern);

    // удаляем лишние слеши в конце

    if (pattern[pattern_length - 1] == '/')
        pattern[pattern_length - 1] = '\0';

    // Разбиваем паттерн по слешу

    uint32_t depth = 1;

    for (char *i = pattern;*i;i++)
    {
        if (*i == '/')
            depth++;
    }

    char *last_index = pattern;
    char **path = malloc(sizeof(char*) * depth);
    uint32_t arr_ind = 0;

    for (char *i = pattern;*i;i++)
    {
        if (*i == '/')  
        {
            *i = '\0';
            path[arr_ind++] = last_index;
            last_index = i + 1;
        }
    }

    path[arr_ind] = last_index;

    // идем по слоям, поддерживаем количество элементов в текущем слое и считаем следующий
    // в каждый момент времени в очереди хранится только два слоя

    dynamic_array_t *search_queue = dynamic_array_create(sizeof(vfs_file_t));
    uint32_t layer_length = 1;
    uint32_t pattern_ind = 0;

    dynamic_array_push_back(search_queue, directory);

    while (search_queue->elements_count != 0 && pattern_ind < depth)
    {
        dynamic_array_t *all_files;
        vfs_file_t *current_dir = dynamic_array_get_bottom(search_queue);

        switch (directory->info->filesystem)
        {
            case VFS_FS_FAT32:
            {
                all_files = fat32_read_directory(&(directory->info->fat32), &(current_dir->fat32));

                if (!all_files)
                {
                    vfs_destroy_files_array(search_queue);
                    free(path);
                    free(pattern);

                    return NULL;
                }

                for (uint32_t i = 0;i < all_files->elements_count;i++)
                {
                    fat32_basic_file_info_t *file = dynamic_array_get_by_index(all_files, i);

                    if (is_matching_pattern(file->filename, path[pattern_ind], ignore_case))
                    {
                        vfs_file_t new_file;
                        memcpy(&(new_file.fat32), file, sizeof(fat32_basic_file_info_t));
                        new_file.info = directory->info;

                        vfs_fill_file_obj_from_fat32(&new_file);

                        if ((new_file.attributes & VFS_ATTRIBUTE_DIRECTORY) && (new_file.fat32.cluster_num == 0))
                            new_file.fat32.cluster_num = 2; // костыль

                        dynamic_array_push_back(search_queue, &new_file);
                    }
                    else
                        free(file->filename);
                }

                dynamic_array_destroy(all_files);

                break;
            }
        }

        dynamic_array_pop_front(search_queue);
        layer_length--;

        if (layer_length == 0)
        {
            layer_length = search_queue->elements_count;
            pattern_ind++;
        }
    }

    free(pattern);
    free(path);

    return search_queue;
}


dynamic_array_t *vfs_find_files(vfs_file_t *directory, const char *_pattern, bool_t ignore_case)
{
    if (_pattern == NULL)
        return vfs_find_files_in_directory(directory, _pattern, ignore_case);
    
    char *pattern = strdup(_pattern);
    char *pattern_start = pattern;

    uint32_t pattern_length = strlen(pattern);

    char *root_name = NULL;
    bool_t return_mountpoints = false;

    if (pattern[0] == '/') // если начинается со слеша, то нужно выполнить для всех точек монтирования с таким именем
    {
        pattern_start++;
        root_name = pattern_start;

        uint32_t pattern_start_ind = strchr_r(pattern_start, '/');

        if (pattern_start_ind != -1)
        {
            pattern_start += pattern_start_ind;
            *pattern_start = '\0';

            pattern_start++;
        }
        else
            return_mountpoints = true;
    }

    dynamic_array_t *result = NULL;

    if (root_name != NULL)
    {
        result = dynamic_array_create(sizeof(vfs_file_t));

        dynamic_array_t *mountpoints = vfs_search_mountpoints(root_name, ignore_case);

        for (uint32_t i = 0;i < mountpoints->elements_count;i++)
        {
            vfs_file_t *mp = dynamic_array_get_by_index(mountpoints, i);

            if (!return_mountpoints)
            {
                dynamic_array_t *search_results = vfs_find_files_in_directory(mp, pattern_start, ignore_case);

                if (!result)
                    result = search_results;
                else
                {
                    if (result->elements_count < search_results->elements_count)
                    {
                        void *_ = result;
                        result = search_results;
                        search_results = _;
                    }

                    for (uint32_t i = 0;i < search_results->elements_count;i++)
                        dynamic_array_push_back(result, dynamic_array_get_by_index(search_results, i));
                    
                    dynamic_array_destroy(search_results);
                }
            }
            else
            {
                dynamic_array_push_back(result, mp);
                vfs_copy_file_obj(dynamic_array_get_top(result), dynamic_array_get_top(result));
            }

            vfs_cleanup_file_obj(mp);
        }

        dynamic_array_destroy(mountpoints);
    }
    else
        result = vfs_find_files_in_directory(directory, pattern_start, ignore_case);

    return result;
}


vfs_stream_t *vfs_open(vfs_file_t *file, uint8_t flags)
{
    vfs_stream_t *result = NULL;

    // проверяем запрещенные комбинации флагов
    if ((flags & VFS_FLAG_READ) & (flags & VFS_FLAG_OVERWRITE) || \
        (flags & VFS_FLAG_READ) & (flags & VFS_FLAG_APPEND) || \
        (flags & VFS_FLAG_OVERWRITE) & (flags & VFS_FLAG_APPEND))
        return NULL;

    switch (file->info->filesystem)
    {
        case VFS_FS_FAT32:
        {
            result = malloc(sizeof(vfs_stream_t));
            memset(result, 0, sizeof(vfs_stream_t));

            result->file = malloc(sizeof(vfs_file_t));
            vfs_copy_file_obj(result->file, file);

            result->flags = flags;

            if (flags & VFS_FLAG_APPEND)
                result->position = result->file->fat32.size;
            else if (flags & VFS_FLAG_OVERWRITE)
                vfs_erase(result->file);

            break;
        }
    }

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


bool_t vfs_erase(vfs_file_t *file)
{
    switch (file->info->filesystem)
    {
        case VFS_FS_FAT32:
        {
            if (!fat32_erase_file_sync(&(file->info->fat32), &(file->fat32)) || \
                !vfs_update_file_obj(file))
                return false;

            break;
        }
    }

    return true;
}


bool_t vfs_newfile(vfs_file_t *dir, const char *filename, uint32_t attributes, vfs_file_t *file)
{
    if (!vfs_is_filename_allowed(filename))
        return false;

    switch (dir->info->filesystem)
    {
        case VFS_FS_FAT32:
        {
            if (!fat32_create_file(&(dir->info->fat32), &(dir->fat32), filename, attributes, file != NULL ? &(file->fat32) : NULL))
                return false;

            if (file)
            {
                file->info = dir->info;

                if (!vfs_fill_file_obj_from_fat32(file))
                {
                    free(file->fat32.filename);
                    return false;
                }
            }

            break;
        }
    }

    return true;
}


bool_t vfs_newdir(vfs_file_t *dir, const char *dirname, uint32_t attributes, vfs_file_t *new_dir)
{
    if (!vfs_is_filename_allowed(dirname))
        return false;

    switch (dir->info->filesystem)
    {
        case VFS_FS_FAT32:
        {
            if (!fat32_create_directory(&(dir->info->fat32), &(dir->fat32), dirname, attributes, new_dir != NULL ? &(new_dir->fat32) : NULL))
                return false;

            if (new_dir)
            {
                new_dir->info = dir->info;

                if (!vfs_fill_file_obj_from_fat32(new_dir))
                {
                    free(new_dir->fat32.filename);
                    return false;
                }
            }
            
            break;
        }
    }

    return true;
}


bool_t vfs_remove(vfs_file_t *file)
{
    switch (file->info->filesystem)
    {
        case VFS_FS_FAT32:
        {
            if (!fat32_remove_file(&(file->info->fat32), &(file->fat32)))
                return false;

            break;
        }
    }

    return true;
}


uint32_t vfs_tell(vfs_stream_t *stream)
{
    return stream->position;
}


void vfs_seek(vfs_stream_t *stream, int offset, uint8_t mode)
{
    switch (mode)
    {
        case VFS_SEEK_MODE_START:
            stream->position = offset;
            break;

        case VFS_SEEK_MODE_END:
            stream->position = stream->file->size - offset;
            break;

        case VFS_SEEK_MODE_CURRENT:
            stream->position = (int)(stream->position) + offset;
    }
}


void vfs_close(vfs_stream_t *stream)
{
    disk_flush_cache_sync(stream->file->info->disk_id);

    vfs_cleanup_file_obj(stream->file);
    free(stream->file);

    free(stream);
}

// TODO: сделать, чтобы fat32 нормально читал по фиксированному количеству байтов, а то щас очень долго n^2
// сделать такие же потоки, как тут
bool_t vfs_read(vfs_stream_t *stream, void *buffer, uint32_t bytes)
{
    if (!(stream->flags & VFS_FLAG_READ))
        return false;
    
    switch (stream->file->info->filesystem)
    {
        case VFS_FS_FAT32:
        {
            if (!fat32_read_file(&(stream->file->info->fat32), &(stream->file->fat32), stream->position, buffer, min(bytes, stream->file->size - stream->position)))
                return false;

            stream->position += bytes;

            return true;
        }
    }

    return false;
}


bool_t vfs_write(vfs_stream_t *stream, void *buffer, uint32_t bytes)
{
    if (!(stream->flags & VFS_FLAG_WRITE))
        return false;
    
    switch (stream->file->info->filesystem)
    {
        case VFS_FS_FAT32:
        {
            if (!fat32_write_file_sync(&(stream->file->info->fat32), &(stream->file->fat32), stream->position, buffer, bytes))
                return false;

            stream->position += bytes;
            vfs_update_file_obj(stream->file);

            return true;
        }
    }

    return false;
}

void vfs_init()
{
    mountpoints = hash_table_create();
    mountpoints_rev = hash_table_create();
}