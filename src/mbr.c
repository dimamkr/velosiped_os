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