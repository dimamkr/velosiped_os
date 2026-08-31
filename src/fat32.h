#ifndef FAT32
#define FAT32

#include "types.h"
#include "disk.h"
#include "datetime.h"
#include "mbr.h"
#include "string.h"
#include "heap.h"

#define FAT32_BAD_CLUSTER 0x0FFFFFF7
#define FAT32_EMPTY_CLUSTER 0x00000000
#define FAT32_LAST_CLUSTER 0x0FFFFFFF

#define FAT32_ATTRIBUTE_READONLY 0x01
#define FAT32_ATTRIBUTE_HIDDEN 0x02
#define FAT32_ATTRIBUTE_SYSTEM 0x04
#define FAT32_ATTRIBUTE_VOLUME_ID 0x08
#define FAT32_ATTRIBUTE_DIRECTORY 0x10
#define FAT32_ATTRIBUTE_ARCHIVE 0x20

#define FAT32_HAS_READING_ERROR(position) (((position).fat_value == FAT32_BAD_CLUSTER) || ((position).fat_value == FAT32_EMPTY_CLUSTER))
#define FAT32_IS_LAST_CLUSTER(position) ((position).fat_value > FAT32_BAD_CLUSTER)

typedef struct {
    byte_t jmp [3];
    char oem [8];

    // Bios Parameter Block
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    // Extended BIOS Parameter Block
    uint32_t sectors_per_fat_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fsinfo_sector;
    uint16_t backup_boot_sector;
    uint32_t reserved [3];
    uint8_t drive_number;
    byte_t reserved2;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label [11];
    char file_system_type [8];

    byte_t boot_code [420];
    uint16_t signature;
} __attribute__((packed)) fat32_initial_sector_t;

typedef struct {
    uint8_t disk_id;
    uint8_t sectors_per_cluster;
    uint16_t fsinfo_sector;
    uint32_t root_cluster;
    uint32_t partition_start;
    uint32_t fat_offset;
    uint32_t fat_size;
    uint32_t data_offset;
    uint8_t fat_count;
} fat32_info_t;

typedef struct {
    char dos_filename [8];
    char dos_extension [3];
    uint8_t attributes;
    byte_t reserved;
    byte_t creation_time_ms;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t cluster_num_high;
    uint16_t last_modify_time;
    uint16_t last_modify_date;
    uint16_t cluster_num_lo;
    uint32_t size;
} __attribute__((packed)) fat32_file_record_t;

typedef struct {
    uint8_t order;
    wchar_t name_part_1 [5];
    uint8_t attributes;
    uint8_t lfn_type;
    uint8_t checksum;
    wchar_t name_part_2 [6];
    uint16_t unused;
    wchar_t name_part_3 [2];
} __attribute__((packed)) fat32_lfn_record_t;

typedef union {
    fat32_lfn_record_t lfn_record;
    fat32_file_record_t file_record;
    byte_t plain_bytes [32];
} fat32_directory_entry_t;

typedef struct {
    byte_t lead_signature [4];
    byte_t reserved [480];
    byte_t struct_signature [4];
    uint32_t free_clusters_count;
    uint32_t next_free_cluster;
    byte_t reserver1 [12];
    byte_t trail_signature [4];
} __attribute__((packed)) fat32_fsinfo_t;

typedef struct {
    uint32_t fat_value;
    uint32_t cluster_num;
} fat32_position_t;

typedef struct {
    char *filename;
    uint32_t attributes;
    datetime_fat_t creation_datetime;
    datetime_fat_t last_modify_datetime;
    uint32_t size;
    uint32_t cluster_num;
    uint32_t entry_cluster_num;
    uint16_t entry_index;
} fat32_basic_file_info_t;

bool_t fat32_read_initial_sector_sync(fat32_info_t *fat_info, fat32_initial_sector_t *init_sector);
bool_t fat32_write_initial_sector_sync(fat32_info_t *fat_info, fat32_initial_sector_t *init_sector);
bool_t fat32_get_info_sync(fat32_info_t *fat_info);
bool_t fat32_get_bootable_partition_info_sync(fat32_info_t *info);
uint32_t fat32_fat_at_sync(fat32_info_t *info, uint32_t fat_index);
bool_t fat32_fat_write_sync(fat32_info_t *info, uint32_t fat_index, uint32_t value);
bool_t fat32_read_cluster_sync(fat32_info_t *info, uint32_t cluster_num, void *buffer);
bool_t fat32_write_cluster_sync(fat32_info_t *info, uint32_t cluster_num, const void *buffer);
void fat32_next_cluster_sync(fat32_info_t *info, fat32_position_t *position);
dynamic_array_t *fat32_read_directory(fat32_info_t *info, fat32_basic_file_info_t *dir_info);
bool_t fat32_read_file(fat32_info_t *info, fat32_basic_file_info_t *file_info, uint32_t start_position, void *buffer, uint32_t buffer_size);
void fat32_mount(fat32_info_t *info, const char *dir_name, fat32_basic_file_info_t *result);
dynamic_array_t *fat32_find_files(fat32_info_t *info, fat32_basic_file_info_t *dir_info, const char *pattern);
bool_t fat32_read_fsinfo_sync(fat32_info_t *info, fat32_fsinfo_t *fsinfo);
bool_t fat32_write_fsinfo_sync(fat32_info_t *info, fat32_fsinfo_t *fsinfo);
uint32_t fat32_take_new_cluster_sync(fat32_info_t *info, uint32_t prev_cluster);
bool_t fat32_release_clusters_sync(fat32_info_t *info, uint32_t start_cluster);
bool_t fat32_update_directory_entry(fat32_info_t *info, fat32_basic_file_info_t *file_info);
bool_t fat32_write_file_sync(fat32_info_t *info, fat32_basic_file_info_t *file_info, uint32_t start_position, void *buffer, uint32_t buffer_size);

#endif