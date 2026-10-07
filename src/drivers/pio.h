#ifndef PIO
#define PIO

#include "types.h"
#include "ata.h"
#include "dynamic_array.h"
#include "system.h"

#define PIO_CHANNEL_PRIMARY 0x1F0
#define PIO_PRIMARY_CONTROL_PORT 0x3F6
#define PIO_CHANNEL_SECONDARY 0x170
#define PIO_SECONDARY_CONTROL_PORT 0x376

#define PIO_MASTER 0xE0
#define PIO_SLAVE 0xF0

#define PIO_REGISTER_DATA 0
#define PIO_REGISTER_ERROR 1
#define PIO_REGISTER_FEATURES 1
#define PIO_REGISTER_SECTORS_COUNT 2
#define PIO_REGISTER_LBA_LO 3
#define PIO_REGISTER_LBA_MID 4
#define PIO_REGISTER_LBA_HI 5
#define PIO_REGISTER_DRIVE_HEAD 6
#define PIO_REGISTER_STATUS 7
#define PIO_REGISTER_COMMAND 7

#define PIO_DISK_TIMEOUT 1000

bool_t pio_wait(uint16_t base);
bool_t pio_identify_sync(byte_t port_num, ata_basic_identify_data_t *result);
bool_t pio_flush_cache_sync(byte_t port_num);
bool_t pio_transfer_sync(byte_t port_num, ata_lba_t lba, uint32_t sectors_count, void *buffer, bool_t write);
dynamic_array_t *pio_enumerate_ports();
void pio_init();

#endif