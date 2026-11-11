#include "mbr.h"

static const char *mbr_partition_types_table [] = {
    [MBR_PARTITION_UNUSED] = "Unused",
    [MBR_PARTITION_FAT12] = "FAT12",
    [MBR_PARTITION_FAT16_L32] = "FAT16 (<32 MB)",
    [MBR_PARTITION_FAT16_G32] = "FAT16",
    [MBR_PARTITION_FAT32_CHS] = "FAT32 (CHS)",
    [MBR_PARTITION_FAT32_LBA] = "FAT32",
    [MBR_PARTITION_EXFAT] = "NTFS/exFAT",
    [MBR_PARTITION_LINUX_SWAP] = "Linux swap",
    [MBR_PARTITION_LINUX_EXT] = "ext2/ext3/ext4",
    [MBR_PARTITION_LINUX_LVM] = "Linux LVM",
    [MBR_PARTITION_GPT] = "GPT-reserved",
    [MBR_PARTITION_ESP] = "EFI System Partition"
};

bool_t mbr_read_sync(uint8_t disk_id, mbr_t *result)
{
    return disk_read_sync(disk_id, 0, 1, result);
}

bool_t mbr_write_sync(uint8_t disk_id, mbr_t *result)
{
    return disk_write_sync(disk_id, 0, 1, result)
        && disk_flush_cache_sync(disk_id);
}

uint32_t mbr_get_bootable_partition(uint8_t disk_id)
{
    mbr_t mbr;
   
    if (mbr_read_sync(disk_id, &mbr))
    {
        for (uint8_t i = 0;i < 4;i++)
        {
            if (mbr.partitions[i].boot_indicator == 0x80)
                return mbr.partitions[i].start_lba;
        }
    }

    return -1;
}

const char *mbr_get_partition_type(uint8_t partition_type)
{
    return mbr_partition_types_table[partition_type];
}