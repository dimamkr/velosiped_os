#include "pci.h"


uint32_t pci_read(uint16_t device_offset, byte_t reg_offset)
{
    uint32_t address = (1 << 31) | (device_offset << 8) | (reg_offset & 0xFC);
    
    outl(PCI_CONFIG_ADDR, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_write(uint16_t device_offset, byte_t reg_offset, uint32_t value)
{
    uint32_t address = (1 << 31) | (device_offset << 8) | (reg_offset & 0xFC);
    
    outl(PCI_CONFIG_ADDR, address);
    outl(PCI_CONFIG_DATA, value);
}

void pci_turn_commands(uint16_t device_offset, uint16_t commands, bool_t enable)
{
    uint32_t command_status_register = pci_read(device_offset, 0x04);
    
    uint16_t command = command_status_register & 0xFFFF;
    uint16_t status = command_status_register >> 16;

    if (enable)
        command |= commands;
    else
        command &= (~commands);

    command_status_register = MAKEDWORD(status, command);
    pci_write(device_offset, 0x04, command_status_register);
}

dynamic_array_t *pci_find_devices(uint32_t find_class_code)
{
    dynamic_array_t *result = dynamic_array_create(sizeof(pci_device_basic_info_t));

    for (uint32_t i = 0;i <= 0xFFFF;i++)
    {
        uint16_t device_offset = i & 0xFFFF;
        pci_device_basic_info_t device_info;
        uint32_t register_data;

        device_info.device_offset = device_offset;

        register_data = pci_read(device_offset, 0x00);
        device_info.vendor_id = register_data & 0xFFFF;
        device_info.device_id = (register_data >> 16) & 0xFFFF;

        if (device_info.vendor_id == 0xFFFF)
            continue;

        register_data = pci_read(device_offset, 0x08);
        device_info.revision_id = register_data & 0xFF;
        device_info.class_code = register_data >> 8;

        if (device_info.class_code != find_class_code && find_class_code != PCI_CLASS_CODE_ALL)
            continue;

        register_data = pci_read(device_offset, 0x04);
        device_info.command = register_data & 0xFFFF;
        device_info.status = register_data >> 16;

        device_info.header_type = (pci_read(device_offset, 0x0C) >> 16) & 0xFF;
        device_info.interrupt_num = pci_read(device_offset, 0x3C) & 0xFF;
        device_info.capabilities_ptr = pci_read(device_offset, 0x34) & 0xFF;

        #pragma GCC unroll 6
        for (uint16_t i = 0;i < 6;i++)
            device_info.bar[i] = pci_read(device_offset, 0x10 + 4*i);

        dynamic_array_push_back(result, &device_info);
    }

    return result;
}