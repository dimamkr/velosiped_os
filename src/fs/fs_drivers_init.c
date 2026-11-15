#include "fs_drivers_init.h"

void fs_drivers_init()
{
    vfs_init();
    fd_init();

    // регистрация fat32 в vfs
    vfs_register_platform(MBR_PARTITION_FAT32_LBA, &fat32_methods);

    // регистрация vfs в fd
    fd_register_platform(NULL, &vfs_methods_table_fd);
}