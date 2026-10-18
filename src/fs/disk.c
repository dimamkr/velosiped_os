#include "disk.h"
#include "konsole.h"
#include "power.h"

#pragma GCC optimize("O0")


static disk_cache_t caches [32] = {0};
byte_t _boot_disk_signature [6] = {0};
static bool_t use_ahci = false;
static uint8_t boot_disk_id = 0xFF;


dynamic_array_t *disk_enumerate_disks()
{
    if (use_ahci)
        return ahci_enumerate_ports();
    else
        return pio_enumerate_ports();
}

bool_t disk_hardware_transfer_sync(uint8_t disk_id, uint32_t start_sector, uint32_t sectors_count, void *buffer, bool_t write)
{
    ata_lba_t lba = {.lba32=start_sector, .lba4=0, .lba5=0};

    if (use_ahci)
        return ahci_transfer_sync(disk_id, lba, sectors_count, buffer, write);
    else
        return pio_transfer_sync(disk_id, lba, sectors_count, buffer, write);
}

// TODO: написать нормальное покрытие кэшами
disk_cache_record_t *disk_get_covering(uint8_t disk_id, uint32_t start_sector, uint32_t sectors_count)
{
    disk_cache_t *cache = caches + disk_id;

    for (linked_list_node_t *cur = cache->cache;cur != NULL;cur = cur->right)
    {
        disk_cache_record_t *record = cur->value;

        if (start_sector >= record->start_sector && start_sector + sectors_count <= record->start_sector + record->sectors_count)
            return record;
    }

    return NULL;
}

disk_cache_record_t *disk_add_cache_record_sync(uint8_t disk_id, uint32_t start_sector, uint32_t sectors_count, const void *data)
{
    if (sectors_count > DISK_MAX_CACHE_BLOCK)
        return NULL;

    disk_cache_t *cache = caches + disk_id;

    disk_flush_cache_sync(disk_id);

    while (cache->cache_capacity + sectors_count > DISK_CACHE_SECTORS_COUNT)
    {
        disk_cache_record_t *record = cache->cache->value;
        cache->cache_capacity -= record->sectors_count;
        linked_list_erase(&(cache->cache), cache->cache);
    }

    linked_list_node_t *top = NULL;
    linked_list_node_t *right = NULL;

    for (linked_list_node_t *cur = cache->cache;cur != NULL;cur = right)
    {
        top = cur;
        right = cur->right;
        disk_cache_record_t *record = cur->value;

        if (record->start_sector < start_sector + sectors_count && start_sector < record->start_sector + record->sectors_count)
        {
            linked_list_erase(&(cache->cache), cur);
            top = cur->left;
        }
    }

    disk_cache_record_t new_record;
    new_record.start_sector = start_sector;
    new_record.sectors_count = sectors_count;

    linked_list_node_t *new_node;
    if (top == NULL)
        new_node = linked_list_add_begin(&(cache->cache), &new_record, sizeof(disk_cache_record_t) + (sectors_count - 1) * 512);
    else
        new_node = linked_list_add(top, &new_record, sizeof(disk_cache_record_t) + (sectors_count - 1) * 512);

    disk_cache_record_t *record = new_node->value;

    memcpy(record->data, data, sectors_count * 512);
    
    return record;
}

bool_t disk_flush_cache_sync(uint8_t disk_id)
{
    disk_cache_t *cache = caches + disk_id;

    for (linked_list_node_t *cur = cache->cache;cur != NULL;cur = cur->right)
    {
        disk_cache_record_t *record = cur->value;

        if (!disk_hardware_transfer_sync(disk_id, record->start_sector, record->sectors_count, record->data, true))
            return false;
    }

    return true;
}

bool_t disk_prefetch_sync(uint8_t disk_id, uint32_t start_sector, uint32_t sectors_count)
{
    return false; // TODO
}

bool_t disk_read_sync(uint8_t disk_id, uint32_t start_sector, uint32_t sectors_count, void *buffer)
{
    disk_cache_record_t *record = disk_get_covering(disk_id, start_sector, sectors_count);

    if (record == NULL)
    {
        if (!disk_hardware_transfer_sync(disk_id, start_sector, sectors_count, buffer, false))
            return false;

        disk_add_cache_record_sync(disk_id, start_sector, sectors_count, buffer);
    }
    else
        memcpy(buffer, record->data + (start_sector - record->start_sector) * 512, sectors_count * 512);

    return true;
}

bool_t disk_write_sync(uint8_t disk_id, uint32_t start_sector, uint32_t sectors_count, void *buffer)
{
    disk_cache_record_t *record = disk_get_covering(disk_id, start_sector, sectors_count);

    if (record == NULL)
    {
        if (!disk_hardware_transfer_sync(disk_id, start_sector, sectors_count, buffer, true))
            return false;

        disk_add_cache_record_sync(disk_id, start_sector, sectors_count, buffer);
    }
    else
        memcpy(record->data + (start_sector - record->start_sector) * 512, buffer, sectors_count * 512);
        
    return true;
}

static void disk_search_boot_disk()
{
    uint8_t buffer [512];

    dynamic_array_t *ports = disk_enumerate_disks();

    for (int i = 0;i < ports->elements_count;i++)
    {
        ata_basic_identify_data_t *identify_data = dynamic_array_get_by_index(ports, i);

        disk_read_sync(identify_data->port_num, 0, 1, buffer);

        if (memcmp(buffer + 440, _boot_disk_signature, 6))
        {
            dynamic_array_destroy(ports);
            boot_disk_id = identify_data->port_num;

            return;
        }
    }

    dynamic_array_destroy(ports);

    boot_disk_id = -1;
}

uint8_t disk_get_boot_disk_id()
{
    return boot_disk_id;
}

void disk_flush_all_caches()
{
    dynamic_array_t *disks = disk_enumerate_disks();

    for (uint32_t i = 0;i < disks->elements_count;i++)
    {
        ata_basic_identify_data_t *disk = dynamic_array_get_by_index(disks, i);

        disk_flush_cache_sync(disk->port_num);
    }

    dynamic_array_destroy(disks);
}

void disk_init()
{
    if (_ahci_supported)
        use_ahci = true;

    disk_search_boot_disk();

    if (boot_disk_id == 0xFF)
    {
        if (use_ahci)
        {
            konsole_set_warning_color();
            konsole_println("Warning: couldn't find booted-from disk via AHCI. Switching to PIO mode.");

            use_ahci = false;

            disk_search_boot_disk();

            if (boot_disk_id == 0xFF)
                goto fail;

            return;
        }

    fail:
        konsole_set_bad_result_color();
        konsole_println("Error: couldn't find booted-from disk.");

        return;
    }

    power_register_shutdown_routine(disk_flush_all_caches);
}