#include "fat32.h"

bool_t fat32_read_initial_sector_sync(fat32_info_t *fat_info, fat32_initial_sector_t *init_sector)
{
    return disk_read_sync(fat_info->disk_id, fat_info->partition_start, 1, init_sector);
}

bool_t fat32_write_initial_sector_sync(fat32_info_t *fat_info, fat32_initial_sector_t *init_sector)
{
    return disk_write_sync(fat_info->disk_id, fat_info->partition_start, 1, init_sector) \
        && disk_flush_cache_sync(fat_info->disk_id);
}

bool_t fat32_get_info_sync(fat32_info_t *fat_info)
{
    fat32_initial_sector_t *init_sector = malloc(sizeof(fat32_initial_sector_t));

    if (fat32_read_initial_sector_sync(fat_info, init_sector))
    {
        fat_info->fat_offset = fat_info->partition_start + init_sector->reserved_sectors;
        fat_info->fat_size = init_sector->fat_count * init_sector->sectors_per_fat_32;
        fat_info->data_offset = fat_info->fat_offset + fat_info->fat_size;
        fat_info->sectors_per_cluster = init_sector->sectors_per_cluster;
        fat_info->fsinfo_sector = init_sector->fsinfo_sector;
        fat_info->root_cluster = init_sector->root_cluster;

        return true;
    }

    return false;
}

