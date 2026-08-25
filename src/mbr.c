#include "mbr.h"

bool_t mbr_read_sync(uint8_t disk_id, mbr_t *result)
{
    return disk_read_sync(disk_id, 0, 1, result);
}

bool_t mbr_write_sync(uint8_t disk_id, mbr_t *result)
{
    return disk_write_sync(disk_id, 0, 1, result) \ 
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