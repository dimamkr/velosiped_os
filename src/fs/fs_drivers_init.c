#include "fs_drivers_init.h"

void fs_drivers_init()
{
    vfs_register_platform(MBR_PARTITION_FAT32_LBA, &fat32_methods);

    vfs_init();
}