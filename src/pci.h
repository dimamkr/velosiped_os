#ifndef PCI
#define PCI

#include "types.h"
#include "dynamic_array.h"

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

#define PCI_CLASS_CODE_ALL 0xFFFFFFFF

#define PCI_CMD_IO_SPACE 0b0000000000000001
#define PCI_CMD_MEM_SPACE 0b0000000000000010
#define PCI_CMD_BUS_MASTER 0b0000000000000100
#define PCI_CMD_SPECIAL_CYCLES 0b0000000000001000
#define PCI_CMD_MEM_WRITE_INVALIDATE 0b0000000000010000
#define PCI_CMD_VGA_PALETTE_SNOOP 0b0000000000100000
#define PCI_CMD_PARITY_ERROR 0b0000000001000000
#define PCI_CMD_SYSTEM_ERROR 0b0000000010000000
#define PCI_CMD_FAST_B2B 0b0000000100000000

#define PCI_MAKE_DEVICE_OFFSET(bus, device, function) ((bus << 8) | (device << 3) | (function))

typedef struct {
    uint16_t device_offset;
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision_id;
    uint32_t class_code;
    uint8_t header_type;
    uint8_t interrupt_num;
    uint8_t capabilities_ptr;
    uint32_t bar[6];
} pci_device_basic_info_t;


uint32_t pci_read(uint16_t device_offset, byte_t reg_offset);
void pci_write(uint16_t device_offset, byte_t reg_offset, uint32_t value);
void pci_turn_commands(uint16_t device_offset, uint16_t commands, bool_t enable);
dynamic_array_t *pci_find_devices(uint32_t find_class_code);


#endif