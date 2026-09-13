#ifndef ATA
#define ATA

#include "types.h"
#include "system.h"

// все команды ATA
#define ATA_CMD_IDENTIFY 0xEC					  // IDENTIFY DEVICE (получить информацию о диске)
#define ATA_CMD_IDENTIFY_PACKET 0xA1			  // IDENTIFY PACKET DEVICE (для ATAPI)
#define ATA_CMD_READ_DMA 0xC8					  // READ DMA (28-bit LBA)
#define ATA_CMD_READ_DMA_EX 0x25				  // READ DMA EXT (48-bit LBA)
#define ATA_CMD_READ_PIO 0x20					  // READ SECTOR(S) (28-bit LBA, PIO)
#define ATA_CMD_READ_PIO_EX 0x24				  // READ SECTOR(S) EXT (48-bit LBA, PIO)
#define ATA_CMD_WRITE_DMA 0xCA					  // WRITE DMA (28-bit LBA)
#define ATA_CMD_WRITE_DMA_EX 0x35				  // WRITE DMA EXT (48-bit LBA)
#define ATA_CMD_WRITE_PIO 0x30					  // WRITE SECTOR(S) (28-bit LBA, PIO)
#define ATA_CMD_WRITE_PIO_EX 0x34				  // WRITE SECTOR(S) EXT (48-bit LBA, PIO)
#define ATA_CMD_FLUSH_CACHE 0xE7				  // FLUSH CACHE
#define ATA_CMD_FLUSH_CACHE_EX 0xEA				  // FLUSH CACHE EXT
#define ATA_CMD_STANDBY 0xE2					  // STANDBY
#define ATA_CMD_STANDBY_IMMEDIATE 0xE0			  // STANDBY IMMEDIATE
#define ATA_CMD_IDLE 0xE3						  // IDLE
#define ATA_CMD_IDLE_IMMEDIATE 0xE1				  // IDLE IMMEDIATE
#define ATA_CMD_SLEEP 0xE6						  // SLEEP
#define ATA_CMD_CHECK_POWER_MODE 0xE5			  // CHECK POWER MODE
#define ATA_CMD_SET_FEATURES 0xEF				  // SET FEATURES
#define ATA_CMD_EXECUTE_DEVICE_DIAGNOSTIC 0x90	  // EXECUTE DEVICE DIAGNOSTIC
#define ATA_CMD_INITIALIZE_DEVICE_PARAMETERS 0x91 // INITIALIZE DEVICE PARAMETERS
#define ATA_CMD_SMART 0xB0						  // SMART (требуется Features = 0xD0 или 0xD1)
#define ATA_CMD_SECURITY_SET_PASSWORD 0xF1
#define ATA_CMD_SECURITY_UNLOCK 0xF2
#define ATA_CMD_SECURITY_ERASE_PREPARE 0xF3
#define ATA_CMD_SECURITY_ERASE_UNIT 0xF4
#define ATA_CMD_SECURITY_FREEZE_LOCK 0xF5
#define ATA_CMD_SECURITY_DISABLE_PASSWORD 0xF6
#define ATA_CMD_DOWNLOAD_MICROCODE 0x92
#define ATA_CMD_NOP 0x00 // No Operation

#define ATA_SIGNATURE 0x00000101

typedef struct
{
	char model[41];
	char serial[21];
	uint64_t sectors;
	bool_t lba48_supported;
	bool_t ncq_supported;
	bool_t dma_supported;
	uint8_t port_num;
	uint32_t port_sig;
} ata_basic_identify_data_t;

typedef struct
{
	union
	{
		uint32_t lba32;
		struct
		{
			byte_t lba0;
			byte_t lba1;
			byte_t lba2;
			byte_t lba3;
		};
	};
	byte_t lba4;
	byte_t lba5;
} ata_lba_t;

void ata_parse_identify_answer(void *cmd_answer_buffer, ata_basic_identify_data_t *result);

#endif