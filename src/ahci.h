#ifndef AHCI
#define AHCI

#include <types.h>
#include <pci.h>
#include <dynamic_array.h>
#include <timer.h>
#include <heap.h>

#define PCI_CLASS_CODE_AHCI 0x010601

#define PCI_PRDT_LENGTH 8

#define	SATA_SIG_ATA 0x00000101		// SATA drive
#define	SATA_SIG_ATAPI 0xEB140101	// SATAPI drive
#define	SATA_SIG_SEMB 0xC33C0101	// Enclosure management bridge
#define	SATA_SIG_PM	0x96690101		// Port multiplier

#define FIS_TYPE_REG_H2D 0x27	// Register FIS - host to device
#define FIS_TYPE_REG_D2H 0x34	// Register FIS - device to host
#define FIS_TYPE_DMA_ACT 0x39	// DMA activate FIS - device to host
#define FIS_TYPE_DMA_SETUP 0x41	// DMA setup FIS - bidirectional
#define FIS_TYPE_DATA 0x46		// Data FIS - bidirectional
#define FIS_TYPE_BIST 0x58		// BIST activate FIS - bidirectional
#define FIS_TYPE_PIO_SETUP 0x5F	// PIO setup FIS - device to host
#define FIS_TYPE_DEV_BITS 0xA1	// Set device bits FIS - device to host

// все команды ATA
#define ATA_CMD_IDENTIFY          0xEC    // IDENTIFY DEVICE (получить информацию о диске)
#define ATA_CMD_IDENTIFY_PACKET   0xA1    // IDENTIFY PACKET DEVICE (для ATAPI)
#define ATA_CMD_READ_DMA          0xC8    // READ DMA (28-bit LBA)
#define ATA_CMD_READ_DMA_EX       0x25    // READ DMA EXT (48-bit LBA)
#define ATA_CMD_READ_PIO          0x20    // READ SECTOR(S) (28-bit LBA, PIO)
#define ATA_CMD_READ_PIO_EX       0x24    // READ SECTOR(S) EXT (48-bit LBA, PIO)
#define ATA_CMD_WRITE_DMA         0xCA    // WRITE DMA (28-bit LBA)
#define ATA_CMD_WRITE_DMA_EX      0x35    // WRITE DMA EXT (48-bit LBA)
#define ATA_CMD_WRITE_PIO         0x30    // WRITE SECTOR(S) (28-bit LBA, PIO)
#define ATA_CMD_WRITE_PIO_EX      0x34    // WRITE SECTOR(S) EXT (48-bit LBA, PIO)
#define ATA_CMD_FLUSH_CACHE       0xE7    // FLUSH CACHE
#define ATA_CMD_FLUSH_CACHE_EX    0xEA    // FLUSH CACHE EXT
#define ATA_CMD_STANDBY           0xE2    // STANDBY
#define ATA_CMD_STANDBY_IMMEDIATE 0xE0    // STANDBY IMMEDIATE
#define ATA_CMD_IDLE              0xE3    // IDLE
#define ATA_CMD_IDLE_IMMEDIATE    0xE1    // IDLE IMMEDIATE
#define ATA_CMD_SLEEP             0xE6    // SLEEP
#define ATA_CMD_CHECK_POWER_MODE  0xE5    // CHECK POWER MODE
#define ATA_CMD_SET_FEATURES      0xEF    // SET FEATURES
#define ATA_CMD_EXECUTE_DEVICE_DIAGNOSTIC 0x90  // EXECUTE DEVICE DIAGNOSTIC
#define ATA_CMD_INITIALIZE_DEVICE_PARAMETERS 0x91  // INITIALIZE DEVICE PARAMETERS
#define ATA_CMD_SMART             0xB0    // SMART (требуется Features = 0xD0 или 0xD1)
#define ATA_CMD_SECURITY_SET_PASSWORD   0xF1
#define ATA_CMD_SECURITY_UNLOCK         0xF2
#define ATA_CMD_SECURITY_ERASE_PREPARE  0xF3
#define ATA_CMD_SECURITY_ERASE_UNIT     0xF4
#define ATA_CMD_SECURITY_FREEZE_LOCK    0xF5
#define ATA_CMD_SECURITY_DISABLE_PASSWORD 0xF6
#define ATA_CMD_DOWNLOAD_MICROCODE 0x92
#define ATA_CMD_NOP                0x00    // No Operation

#define ATA_ERROR_ANY 0xF9000000

typedef volatile struct {
	uint32_t clb;		// 0x00, command list base address, 1K-byte aligned
	uint32_t clbu;		// 0x04, command list base address upper 32 bits
	uint32_t fb;		// 0x08, FIS base address, 256-byte aligned
	uint32_t fbu;		// 0x0C, FIS base address upper 32 bits
	uint32_t is;		// 0x10, interrupt status
	uint32_t ie;		// 0x14, interrupt enable
	uint32_t cmd;		// 0x18, command and status
	uint32_t rsv0;		// 0x1C, Reserved
	uint32_t tfd;		// 0x20, task file data
	uint32_t sig;		// 0x24, signature
	uint32_t ssts;		// 0x28, SATA status (SCR0:SStatus)
	uint32_t sctl;		// 0x2C, SATA control (SCR2:SControl)
	uint32_t serr;		// 0x30, SATA error (SCR1:SError)
	uint32_t sact;		// 0x34, SATA active (SCR3:SActive)
	uint32_t ci;		// 0x38, command issue
	uint32_t sntf;		// 0x3C, SATA notification (SCR4:SNotification)
	uint32_t fbs;		// 0x40, FIS-based switch control
	uint32_t rsv1[11];	// 0x44 ~ 0x6F, Reserved
	uint32_t vendor[4];	// 0x70 ~ 0x7F, vendor specific
} __attribute__((packed)) ahci_hba_port_t;

typedef volatile struct {
	// 0x00 - 0x2B, Generic Host Control
	uint32_t cap;		// 0x00, Host capability
	uint32_t ghc;		// 0x04, Global host control
	uint32_t is;		// 0x08, Interrupt status
	uint32_t pi;		// 0x0C, Port implemented
	uint32_t vs;		// 0x10, Version
	uint32_t ccc_ctl;	// 0x14, Command completion coalescing control
	uint32_t ccc_pts;	// 0x18, Command completion coalescing ports
	uint32_t em_loc;		// 0x1C, Enclosure management location
	uint32_t em_ctl;		// 0x20, Enclosure management control
	uint32_t cap2;		// 0x24, Host capabilities extended
	uint32_t bohc;		// 0x28, BIOS/OS handoff control and status

	// 0x2C - 0x9F, Reserved
	uint8_t rsv[0xA0-0x2C];

	// 0xA0 - 0xFF, Vendor specific registers
	uint8_t vendor[0x100-0xA0];

	// 0x100 - 0x10FF, Port control registers
	ahci_hba_port_t	ports[1];	// 1 ~ 32
} __attribute__((packed)) ahci_hba_mem_t;


typedef struct {
    uint8_t cfl:5;          // Command FIS length
    uint8_t atapi:1;        // A
    uint8_t write:1;        // W
    uint8_t prefetchable:1; // P
    uint8_t reset:1;        // R
    uint8_t bist:1;         // B
    uint8_t clear_busy:1;   // C
    uint8_t reserved:1;
    uint8_t pmp:4;          // Port multiplier
    uint16_t prdtl;       // Number of PRDT entries
    volatile uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved2[4];
} __attribute__((packed)) ahci_cmd_header_t;

typedef struct {
    uint32_t dba;          // Физический адрес буфера данных
    uint32_t dbau;         // Старшая часть (64-bit)
    uint32_t reserved;
    uint32_t dbc:22;       // Количество байт (макс. 4 МБ) - 1
    uint32_t reserved2:9;
    uint32_t i:1; // Прерывание
} __attribute__((packed)) ahci_prdt_t;

typedef struct {
    uint8_t cfis[64];      // Command FIS (64 байта)
    uint8_t acmd[16];      // ATAPI-команда (если нужно)
    uint8_t reserved[48];
    ahci_prdt_t prdt[PCI_PRDT_LENGTH];   // PRDT
} __attribute__((packed)) ahci_cmd_table_t;

typedef struct {
	uint8_t  fis_type;  // FIS_TYPE_REG_H2D
	uint8_t  pmport:4;  // Port multiplier
	uint8_t  rsv0:3;    // Reserved
	uint8_t  c:1;       // 1: Command, 0: Control
	uint8_t  command;   // Command register
	uint8_t  featurel;  // Feature register, 7:0
	uint8_t  lba0;		// LBA low register, 7:0
	uint8_t  lba1;		// LBA mid register, 15:8
	uint8_t  lba2;		// LBA high register, 23:16
	uint8_t  device;	// Device register
	uint8_t  lba3;		// LBA register, 31:24
	uint8_t  lba4;		// LBA register, 39:32
	uint8_t  lba5;		// LBA register, 47:40
	uint8_t  featureh;	// Feature register, 15:8
	uint8_t  countl;    // Count register, 7:0
	uint8_t  counth;    // Count register, 15:8
	uint8_t  icc;       // Isochronous command completion
	uint8_t  control;	// Control register
	uint32_t  reserved;
} __attribute__((packed)) ahci_fis_h2d_t;

typedef struct {
    char model[41];
    char serial[21];
    uint64_t sectors;
    bool_t lba48_supported;
    bool_t ncq_supported;
    bool_t dma_supported;
	uint8_t port_num;
	uint32_t port_sig;
} ahci_basic_identify_data_t;

typedef struct {
	union {
		uint32_t lba32;
		struct {
			byte_t lba0;
			byte_t lba1;
			byte_t lba2;
			byte_t lba3;
		};
	};
	byte_t lba4;
	byte_t lba5;
} ahci_lba_t;

bool_t ahci_init();
bool_t ahci_init_port(byte_t port_num);
uint8_t find_free_command_slot(byte_t port_num);
bool_t ahci_identify_sync(byte_t port_num, ahci_basic_identify_data_t *result);
bool_t ahci_flush_cache_sync(byte_t port_num);
bool_t ahci_transfer_sync(byte_t port_num, ahci_lba_t lba, uint32_t sectors_count, void *buffer, bool_t write);
dynamic_array_t *ahci_enumerate_ports();

extern bool_t _ahci_supported;

#endif