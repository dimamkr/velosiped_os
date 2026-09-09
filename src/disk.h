#ifndef DISK
#define DISK

#include "ahci.h"
#include "types.h"
#include "heap.h"
#include "linked_list.h"

#define DISK_CACHE_SECTORS_COUNT 65536 // размер кэша в секторах
#define DISK_MAX_CACHE_BLOCK 4096 // максимальный размер непрерывного блока, который можно положить в кэш

// TODO: написать префетчинг и распознавание паттернов

typedef struct {
    uint32_t start_sector;
    uint32_t sectors_count;
    byte_t data [512]; // переменный размер
} __attribute__((packed)) disk_cache_record_t;

typedef struct {
    uint32_t start_sector_prev;
    uint32_t start_sector_prev_prev;
    uint32_t sectors_count_prev;
    uint32_t sectors_count_prev_prev;
    uint32_t cache_capacity;
    linked_list_node_t *cache; // TODO: написать быструю структуру для кэширования
} disk_cache_t;

bool_t disk_hardware_transfer_sync(uint8_t disk_id, uint32_t start_sector, uint32_t sectors_count, void *buffer, bool_t write);
disk_cache_record_t *disk_get_covering(uint8_t disk_id, uint32_t start_sector, uint32_t sectors_count);
disk_cache_record_t *disk_add_cache_record_sync(uint8_t disk_id, uint32_t start_sector, uint32_t sectors_count, const void *data);
bool_t disk_flush_cache_sync(uint8_t disk_id);
bool_t disk_prefetch_sync(uint8_t disk_id, uint32_t start_sector, uint32_t sectors_count);
bool_t disk_read_sync(uint8_t disk_id, uint32_t start_sector, uint32_t sectors_count, void *buffer);
bool_t disk_write_sync(uint8_t disk_id, uint32_t start_sector, uint32_t sectors_count, void *buffer);
uint8_t disk_get_boot_disk_id();

extern byte_t _boot_disk_signature [6];

#endif
