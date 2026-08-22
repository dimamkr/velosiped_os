#ifndef MBR
#define MBR

#include "types.h"
#include "disk.h"

#define MBR_PARTITION_UNUSED 0x00
#define MBR_PARTITION_FAT12 0x01
#define MBR_PARTITION_FAT16_L32 0x04 // fat16 (<32MB)
#define MBR_PARTITION_FAT16_G32 0x06 // fat16 (>32MB)
#define MBR_PARTITION_FAT32_CHS 0x0B
#define MBR_PARTITION_FAT32_LBA 0x0C
#define MBR_PARTITION_EXFAT 0x07 // NTFS/exFAT
#define MBR_PARTITION_LINUX_SWAP 0x82
#define MBR_PARTITION_LINUX_EXT 0x83 // ext2/ext3/ext4
#define MBR_PARTITION_LINUX_LVM 0x8E
#define MBR_PARTITION_GPT 0xEE
#define MBR_PARTITION_ESP 0xEF // EFI System Partition

typedef struct {
    byte_t boot_indicator;
    uint8_t start_head;
    uint8_t start_sector_cyl;
    uint8_t start_cyl;
    byte_t partition_type;
    uint8_t end_head;
    uint8_t end_sector_cyl;
    uint8_t end_cyl;
    uint32_t start_lba;
    uint32_t total_sectors;
} __attribute__((packed)) mbr_partition_entry_t;

typedef struct {
    byte_t bootloader [446];
    mbr_partition_entry_t partitions [4];
    uint16_t signature;
} __attribute__((packed)) mbr_t;

bool_t mbr_read_sync(uint8_t disk_id, mbr_t *result);
bool_t mbr_write_sync(uint8_t disk_id, mbr_t *result);

#endif