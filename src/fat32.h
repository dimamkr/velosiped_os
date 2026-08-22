#ifndef FAT32
#define FAT32

#include "types.h"
#include "disk.h"

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
} fat32_info_t;

bool_t fat32_read_initial_sector_sync(fat32_info_t *fat_info, fat32_initial_sector_t *init_sector);
bool_t fat32_write_initial_sector_sync(fat32_info_t *fat_info, fat32_initial_sector_t *init_sector);
bool_t fat32_get_info_sync(fat32_info_t *fat_info);

#endif