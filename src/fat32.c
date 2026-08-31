#include "fat32.h"

bool_t fat32_read_initial_sector_sync(fat32_info_t *info, fat32_initial_sector_t *init_sector)
{
    return disk_read_sync(info->disk_id, info->partition_start, 1, init_sector);
}

bool_t fat32_write_initial_sector_sync(fat32_info_t *info, fat32_initial_sector_t *init_sector)
{
    return disk_write_sync(info->disk_id, info->partition_start, 1, init_sector) \
        && disk_flush_cache_sync(info->disk_id);
}

bool_t fat32_get_info_sync(fat32_info_t *info)
{
    fat32_initial_sector_t *init_sector = malloc(sizeof(fat32_initial_sector_t));

    if (fat32_read_initial_sector_sync(info, init_sector))
    {
        info->fat_offset = info->partition_start + init_sector->reserved_sectors;
        info->fat_size = init_sector->sectors_per_fat_32;
        info->fat_count = init_sector->fat_count;
        info->data_offset = info->fat_offset + info->fat_size * info->fat_count;
        info->sectors_per_cluster = init_sector->sectors_per_cluster;
        info->fsinfo_sector = init_sector->fsinfo_sector;
        info->root_cluster = init_sector->root_cluster;

        return true;
    }

    return false;
}

bool_t fat32_get_bootable_partition_info_sync(fat32_info_t *info)
{
    info->disk_id = disk_get_boot_disk_id();
    info->partition_start = mbr_get_bootable_partition(info->disk_id);

    return fat32_get_info_sync(info);
}

uint32_t fat32_fat_at_sync(fat32_info_t *info, uint32_t fat_index)
{
    uint32_t fat_sector [128];

    uint32_t sector_offset = fat_index / 128;
    uint8_t record_offset = fat_index % 128;

    if (!disk_read_sync(info->disk_id, info->fat_offset + sector_offset, 1, fat_sector))
        return 0x0FFFFFF7;

    return fat_sector[record_offset] & 0x0FFFFFFF;
}

bool_t fat32_fat_write_sync(fat32_info_t *info, uint32_t fat_index, uint32_t value)
{
    uint32_t fat_sector [128];

    uint32_t sector_offset = fat_index / 128;
    uint8_t record_offset = fat_index % 128;

    if (!disk_read_sync(info->disk_id, info->fat_offset + sector_offset, 1, fat_sector))
        return false;

    fat_sector[record_offset] = value;

    if (!disk_write_sync(info->disk_id, info->fat_offset + sector_offset, 1, fat_sector))
        return false;

    if (__builtin_expect(info->fat_count == 2, true))
    {
        if (!disk_write_sync(info->disk_id, info->fat_offset + info->fat_size + sector_offset, 1, fat_sector))
            return false;
    }

    return true;
}

bool_t fat32_read_cluster_sync(fat32_info_t *info, uint32_t cluster_num, void *buffer)
{
    uint32_t cluster_start = info->data_offset + (cluster_num - 2) * info->sectors_per_cluster;

    return disk_read_sync(info->disk_id, cluster_start, info->sectors_per_cluster, buffer);
}

bool_t fat32_write_cluster_sync(fat32_info_t *info, uint32_t cluster_num, const void *buffer)
{
    uint32_t cluster_start = info->data_offset + (cluster_num - 2) * info->sectors_per_cluster;

    return disk_write_sync(info->disk_id, cluster_start, info->sectors_per_cluster, buffer);
}

void fat32_next_cluster_sync(fat32_info_t *info, fat32_position_t *position)
{
    position->cluster_num = position->fat_value;
    position->fat_value = fat32_fat_at_sync(info, position->cluster_num);
}

dynamic_array_t *fat32_read_directory(fat32_info_t *info, fat32_basic_file_info_t *dir_info)
{
    fat32_position_t position = {0};
    position.cluster_num = dir_info->cluster_num;
    position.fat_value = fat32_fat_at_sync(info, position.cluster_num);

    dynamic_array_t *result = dynamic_array_create(sizeof(fat32_basic_file_info_t));
    dynamic_array_t *lfn_entries = dynamic_array_create(sizeof(fat32_lfn_record_t));

    uint32_t entries_per_cluster = 16 * info->sectors_per_cluster;
    fat32_directory_entry_t *buffer = malloc(32 * entries_per_cluster);

    for (;!FAT32_HAS_READING_ERROR(position);fat32_next_cluster_sync(info, &position))
    {
        if (!fat32_read_cluster_sync(info, position.cluster_num, buffer))
        {
            free(buffer);
            dynamic_array_destroy(result);
            dynamic_array_destroy(lfn_entries);
            return NULL;
        }

        for (uint32_t i = 0;i < entries_per_cluster;i++)
        {
            byte_t entry_type = buffer[i].plain_bytes[0x0B];

            if (buffer[i].plain_bytes[0] == 0)
                break;

            switch (entry_type)
            {
                case 0xE5: // удаленная запись
                    continue;

                case 0x0F: // lfn-запись
                    dynamic_array_push_front(lfn_entries, buffer + i);
                    break;

                default:
                {
                    fat32_basic_file_info_t file_info;

                    if (lfn_entries->elements_count != 0)
                    {
                        uint32_t wide_filename_size = sizeof(wchar_t) * (13 * lfn_entries->elements_count + 1);
                        wchar_t *wide_filename = malloc(wide_filename_size);
                        memset(wide_filename, 0, wide_filename_size);

                        for (uint8_t i = 0;i < lfn_entries->elements_count;i++)
                        {
                            fat32_lfn_record_t *lfn_record = dynamic_array_get_by_index(lfn_entries, i);

                            memcpy(wide_filename + i * 13,
                                lfn_record->name_part_1,
                                sizeof(lfn_record->name_part_1));
                            memcpy(wide_filename + i * 13 + 5,
                                lfn_record->name_part_2,
                                sizeof(lfn_record->name_part_2));
                            memcpy(wide_filename + i * 13 + 11,
                                lfn_record->name_part_3,
                                sizeof(lfn_record->name_part_3));
                        }

                        file_info.filename = malloc(wide_filename_size);
                        realloc(file_info.filename, wide_char_to_utf8(file_info.filename, wide_filename, wide_filename_size) + 1);
                        free(wide_filename);
                        dynamic_array_clear(lfn_entries);
                    }
                    else
                    {
                        uint32_t dos_filename_size = 8;
                        for (;dos_filename_size > 0 && buffer[i].file_record.dos_filename[dos_filename_size - 1] == ' ';dos_filename_size--);
                        uint32_t dos_extension_size = 3;
                        for (;dos_extension_size > 0 && buffer[i].file_record.dos_extension[dos_extension_size - 1] == ' ';dos_extension_size--);

                        bool_t dot = ((dos_filename_size != 0) && (dos_extension_size != 0));
                        file_info.filename = malloc(dos_filename_size + dos_extension_size + dot + 1);

                        memcpy(file_info.filename, buffer[i].file_record.dos_filename, dos_filename_size);
                        if (dot)
                            file_info.filename[dos_filename_size] = '.';
                        memcpy(file_info.filename + dos_filename_size + dot, buffer[i].file_record.dos_extension, dos_extension_size);
                        file_info.filename[dos_filename_size + dot + dos_extension_size] = '\0';
                    }

                    file_info.attributes = buffer[i].file_record.attributes;
                    file_info.creation_datetime.date=buffer[i].file_record.creation_date;
                    file_info.creation_datetime.time=buffer[i].file_record.creation_time;
                    file_info.last_modify_datetime.date = buffer[i].file_record.last_modify_date;
                    file_info.last_modify_datetime.time = buffer[i].file_record.last_modify_time;
                    file_info.cluster_num = ((uint32_t)(buffer[i].file_record.cluster_num_high) << 16) + buffer[i].file_record.cluster_num_lo;
                    file_info.size = buffer[i].file_record.size;
                    file_info.entry_cluster_num = position.cluster_num;
                    file_info.entry_index = i;

                    dynamic_array_push_back(result, &file_info);
                    
                    break;
                }
            }
        }

        if (FAT32_IS_LAST_CLUSTER(position))
            break;
    }

    dynamic_array_destroy(lfn_entries);
    free(buffer);

    if (FAT32_HAS_READING_ERROR(position))
    {
        for (uint32_t i = 0;i < result->elements_count;i++)
        {
            fat32_basic_file_info_t *file_info = dynamic_array_get_by_index(result, i);
            free(file_info->filename);
        }

        dynamic_array_destroy(result);
        return NULL;
    }

    return result;
}

bool_t fat32_read_file(fat32_info_t *info, fat32_basic_file_info_t *file_info, uint32_t start_position, void *buffer, uint32_t buffer_size)
{
    uint32_t cluster_size = info->sectors_per_cluster * 512;

    fat32_position_t position = {0};
    position.cluster_num = file_info->cluster_num;
    position.fat_value = fat32_fat_at_sync(info, position.cluster_num);

    // пропускаем кластеры до начала
    while (start_position >= cluster_size)
    {
        if (FAT32_IS_LAST_CLUSTER(position) || FAT32_HAS_READING_ERROR(position))
            return false;

        fat32_next_cluster_sync(info, &position);
        start_position -= cluster_size;
    }

    void *temp_buffer = malloc(cluster_size);
    uint32_t free_buffer_size = min(buffer_size, file_info->size);
    uint32_t buffer_index = 0;

    for (;!FAT32_HAS_READING_ERROR(position);fat32_next_cluster_sync(info, &position))
    {
        if (!fat32_read_cluster_sync(info, position.cluster_num, temp_buffer))
        {
            free(temp_buffer);
            return false;
        }

        uint32_t bytes_to_read = min(cluster_size - start_position, free_buffer_size);

        memcpy(buffer + buffer_index, temp_buffer + start_position, bytes_to_read);
        buffer_index += bytes_to_read;
        start_position = 0;

        free_buffer_size -= bytes_to_read;

        if (free_buffer_size == 0)
            break;

        if (FAT32_IS_LAST_CLUSTER(position))
            break;
    }

    free(temp_buffer);

    if (FAT32_HAS_READING_ERROR(position))
        return false;

    return true;
}

void fat32_mount(fat32_info_t *info, const char *dir_name, fat32_basic_file_info_t *result)
{
    result->attributes = FAT32_ATTRIBUTE_DIRECTORY | FAT32_ATTRIBUTE_SYSTEM;
    result->cluster_num = info->root_cluster;
    result->size = 0;
    result->filename = strdup(dir_name);
}

dynamic_array_t *fat32_find_files(fat32_info_t *info, fat32_basic_file_info_t *dir_info, const char *pattern)
{
    dynamic_array_t *result = dynamic_array_create(sizeof(fat32_basic_file_info_t));
    dynamic_array_t *files = fat32_read_directory(info, dir_info);

    if (files == NULL)
    {
        dynamic_array_destroy(result);
        return NULL;
    }
    if (pattern == NULL)
    {
        dynamic_array_destroy(result);
        return files;
    }

    for (uint32_t i = 0;i < files->elements_count;i++)
    {
        fat32_basic_file_info_t *entry = dynamic_array_get_by_index(files, i);

        if (is_matching_pattern(entry->filename, pattern))
            dynamic_array_push_back(result, entry);
        else
            free(entry->filename);
    }

    dynamic_array_destroy(files);

    return result;
}

bool_t fat32_read_fsinfo_sync(fat32_info_t *info, fat32_fsinfo_t *fsinfo)
{
    if (info->fsinfo_sector == 0)
        return false;

    uint32_t fsinfo_sector_offset = info->partition_start + info->fsinfo_sector;

    return disk_read_sync(info->disk_id, fsinfo_sector_offset, 1, fsinfo);
}

bool_t fat32_write_fsinfo_sync(fat32_info_t *info, fat32_fsinfo_t *fsinfo)
{
    if (info->fsinfo_sector == 0)
        return false;

    uint32_t fsinfo_sector_offset = info->partition_start + info->fsinfo_sector;

    return disk_write_sync(info->disk_id, fsinfo_sector_offset, 1, fsinfo)
        && disk_flush_cache_sync(info->disk_id);
}

uint32_t fat32_take_new_cluster_sync(fat32_info_t *info, uint32_t prev_cluster)
{
    uint32_t current_cluster = info->root_cluster;
    fat32_fsinfo_t *fsinfo = NULL;

    if (info->fsinfo_sector)
    {
        info = malloc(sizeof(fat32_fsinfo_t));
        fat32_read_fsinfo_sync(info, fsinfo);
        current_cluster = fsinfo->next_free_cluster;
    }
    
    uint32_t fat_sector [128];
    disk_read_sync(info->disk_id, info->fat_offset + current_cluster / 128, 1, fat_sector);

    uint32_t taken_cluster = 0;

    for (;;current_cluster++)
    {
        if (current_cluster % 128 == 0)
            disk_read_sync(info->disk_id, info->fat_offset + current_cluster / 128, 1, fat_sector);

        if ((fat_sector[current_cluster % 128] & 0x0FFFFFFF) == FAT32_EMPTY_CLUSTER)
        {
            if (taken_cluster)
                break;
            taken_cluster = current_cluster;
        }
    }

    if (prev_cluster)
        fat32_fat_write_sync(info, prev_cluster, taken_cluster);
    fat32_fat_write_sync(info, taken_cluster, FAT32_LAST_CLUSTER);

    if (info->fsinfo_sector)
    {
        fsinfo->free_clusters_count--;
        fsinfo->next_free_cluster = current_cluster;
        fat32_write_fsinfo_sync(info, fsinfo);
        free(fsinfo);
    }

    return taken_cluster;
}

bool_t fat32_release_clusters_sync(fat32_info_t *info, uint32_t start_cluster)
{
    uint32_t min_cluster = 0xFFFFFFFF;
    fat32_fsinfo_t *fsinfo = NULL;

    if (info->fsinfo_sector)
    {
        info = malloc(sizeof(fat32_fsinfo_t));
        fat32_read_fsinfo_sync(info, fsinfo);
        min_cluster = fsinfo->next_free_cluster;
    }

    fat32_position_t position = {0};
    position.cluster_num = start_cluster;
    position.fat_value = fat32_fat_at_sync(info, position.cluster_num);

    for (;!FAT32_HAS_READING_ERROR(position);fat32_next_cluster_sync(info, &position))
    {
        if (position.cluster_num < min_cluster)
            min_cluster = position.cluster_num;
            
        fat32_fat_write_sync(info, position.cluster_num, FAT32_EMPTY_CLUSTER);

        if (FAT32_IS_LAST_CLUSTER(position))
            break;
    }

    return true;
}

bool_t fat32_update_directory_entry(fat32_info_t *info, fat32_basic_file_info_t *file_info)
{
    fat32_directory_entry_t entries [16];
    uint32_t entry_sector = info->data_offset + (file_info->entry_cluster_num - 2) * info->sectors_per_cluster + file_info->entry_index / 16;
    uint8_t entry_index = file_info->entry_index % 16;

    if (!disk_read_sync(info->disk_id, entry_sector, 1, entries))
        return false;

    datetime_t modified_dt;
    datetime_get(&modified_dt);
    datetime_fat_t modified_fat_dt;
    datetime_fat_from_datetime(&modified_dt, &modified_fat_dt);

    entries[entry_index].file_record.attributes = file_info->attributes;
    entries[entry_index].file_record.size = file_info->size;
    entries[entry_index].file_record.cluster_num_lo = file_info->cluster_num & 0xFFFF;
    entries[entry_index].file_record.cluster_num_high = file_info->cluster_num >> 16;
    entries[entry_index].file_record.creation_date = file_info->creation_datetime.date;
    entries[entry_index].file_record.creation_time = file_info->creation_datetime.time;
    entries[entry_index].file_record.last_modify_date = modified_fat_dt.date;
    entries[entry_index].file_record.last_modify_time = modified_fat_dt.time;
    
    if (!disk_write_sync(info->disk_id, entry_sector, 1, entries))
        return false;

    return true;
}

// отличие от чтения только в возможном изменении размера и в том, что мы попутно выделяем новые кластеры
bool_t fat32_write_file_sync(fat32_info_t *info, fat32_basic_file_info_t *file_info, uint32_t start_position, void *buffer, uint32_t buffer_size)
{
    uint32_t cluster_size = info->sectors_per_cluster * 512;

    fat32_basic_file_info_t new_file_info;
    memcpy(&new_file_info, file_info, sizeof(fat32_basic_file_info_t));

    fat32_position_t position = {0};

    if (file_info->cluster_num)
    {
        position.cluster_num = file_info->cluster_num;
        position.fat_value = fat32_fat_at_sync(info, position.cluster_num);
    }
    else
    {
        position.cluster_num = fat32_take_new_cluster_sync(info, 0);
        position.fat_value = FAT32_LAST_CLUSTER;
    }

    uint32_t current_position = start_position;

    // пропускаем кластеры до начала, попутно создавая новые
    while (current_position >= cluster_size)
    {
        if (FAT32_HAS_READING_ERROR(position))
            return false;

        if (FAT32_IS_LAST_CLUSTER(position))
            position.cluster_num = fat32_take_new_cluster_sync(info, position.cluster_num);
        else
            fat32_next_cluster_sync(info, &position);

        current_position -= cluster_size;
    }

    void *temp_buffer = malloc(cluster_size * 512);
    uint32_t non_writed_buffer_size = buffer_size;
    uint32_t buffer_index = 0;

    for (;!FAT32_HAS_READING_ERROR(position);)
    {
        if (!fat32_read_cluster_sync(info, position.cluster_num, temp_buffer))
        {
            free(temp_buffer);
            return false;
        }

        uint32_t bytes_to_write = min(cluster_size - current_position, non_writed_buffer_size);

        memcpy(temp_buffer + current_position, buffer + buffer_index, bytes_to_write);

        if (!fat32_write_cluster_sync(info, position.cluster_num, temp_buffer))
        {
            free(temp_buffer);
            return false;
        }

        buffer_index += bytes_to_write;
        current_position = 0;

        non_writed_buffer_size -= bytes_to_write;

        if (non_writed_buffer_size == 0)
            break;

        if (FAT32_IS_LAST_CLUSTER(position))
            position.cluster_num = fat32_take_new_cluster_sync(info, position.cluster_num);
        else
            fat32_next_cluster_sync(info, &position);
    }

    free(temp_buffer);

    new_file_info.size = max(file_info->size, start_position + buffer_size);

    if (!fat32_update_directory_entry(info, &new_file_info))
        return false;

    return true;
}