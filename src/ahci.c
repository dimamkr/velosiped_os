#include "ahci.h"


static ahci_hba_mem_t *hba;

static ahci_cmd_header_t *cmd_list[32];
static ahci_cmd_table_t *cmd_table[32];
static void *received_fis[32];


bool_t ahci_init()
{
    // ищем устройство AHCI 
    dynamic_array_t *ahci_devices = pci_find_devices(PCI_CLASS_CODE_AHCI);

    if (ahci_devices->elements_count == 0)
    {
        konsole_println("AHCI not found");
        dynamic_array_destroy(ahci_devices);
        return false;
    }

    pci_device_basic_info_t *base_ahci_info = dynamic_array_get_by_index(ahci_devices, 0);

    // включаем DMA, доступ к памяти и IO
    pci_turn_commands(base_ahci_info->device_offset, PCI_CMD_BUS_MASTER, true);
    pci_turn_commands(base_ahci_info->device_offset, PCI_CMD_MEM_SPACE, true);
    pci_turn_commands(base_ahci_info->device_offset, PCI_CMD_IO_SPACE, true);

    hba = (ahci_hba_mem_t*)(base_ahci_info->bar[5] & 0xFFFFFFF0); // находим регистры HBA по адресу из bar5

    dynamic_array_destroy(ahci_devices);

    hba->ghc |= (1 << 31); // включаем AHCI контроллер

    for (byte_t i = 0;i < 32;i++)
    {
        if (hba->pi & (1 << i)) // проверяем, реализован ли i-й порт
            ahci_init_port(i); 
    }

    return true;
}

bool_t ahci_init_port(byte_t port_num)
{
    ahci_hba_port_t *port = &(hba->ports[port_num]);

    if ((port->ssts & 0x0000000F) != 0x3) // проверяем det, 0x3 - устройство готово
        return false;

    switch (port->sig) // проверяем, что за устройство подключено по порту
    {
        case SATA_SIG_ATA:
        case SATA_SIG_ATAPI:
        case SATA_SIG_SEMB:
        case SATA_SIG_PM:
            break;
        default:
            return false;
    }

    // выделяем выровненное место под структуры согласно спецификации
    // наглядно: https://wiki.osdev.org/AHCI
    cmd_list[port_num] = alligned_malloc(32 * sizeof(ahci_cmd_header_t), 1024);
    cmd_table[port_num] = alligned_malloc(32 * sizeof(ahci_cmd_table_t), 128);
    received_fis[port_num] = alligned_malloc(256, 256);

    port->cmd &= ~((1 << 0) | (1 << 4)); // остановить порт (очистили ST и FRE)
    while (port->cmd & (1 << 14) || port->cmd & (1 << 15)); // ждем окончания приема fis и обработки запущенных команд

    port->clb = (uint32_t)cmd_list[port_num];
    port->clbu = 0;
    port->fb = (uint32_t)received_fis[port_num];
    port->fbu = 0;

    for (uint8_t i = 0;i < 32;i++)
    {
        cmd_list[port_num][i].ctba = (uint32_t)(cmd_table[port_num] + i);
        cmd_list[port_num][i].ctbau = 0;
    }

    while (port->cmd & (1 << 15)); // ждем окончания выполнения команд
    port->cmd |= (1 << 0) | (1 << 4); // запустить порт

    return true;
}

uint8_t find_free_command_slot(byte_t port_num)
{
    ahci_hba_port_t *port = &(hba->ports[port_num]);

    for (uint8_t i = 0;i < 32;i++)
    {
        if (port->ci & (1 << i) | port->sact & (1 << i)) // если на порту выполняется команда или NCQ, пропускаем
            continue;
        return i;
    }

    return 0xFF;
}

ahci_basic_identify_data_t *ahci_identify(byte_t port_num)
{
    const uint32_t cmd_answer_buffer_size = 512; // размер ответа на команду IDENTIFY
    ahci_hba_port_t *port = &(hba->ports[port_num]);

    uint8_t cmd_num = find_free_command_slot(port_num); // ищем свободный слот для команды

    ahci_cmd_header_t *command_header = (ahci_cmd_header_t*)port->clb + cmd_num;
    ahci_cmd_table_t *command_table = (ahci_cmd_table_t*)(command_header->ctba);

    memset(command_table, 0, sizeof(ahci_cmd_table_t));

    byte_t *cmd_answer_buffer = malloc(cmd_answer_buffer_size); // выделяем буффер под ответ

    // заполняем prdt entry (достаточно одной записи)
    command_table->prdt[0].dba = (uint32_t)cmd_answer_buffer;
    command_table->prdt[0].dbc = cmd_answer_buffer_size - 1;
    
    ahci_fis_h2d_t *fis = (ahci_fis_h2d_t*)(command_table->cfis);

    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1; // устанавливаем бит команды
    fis->command = ATA_CMD_IDENTIFY;
    fis->countl = 1; // одна команда
    
    command_header->cfl = sizeof(ahci_fis_h2d_t) / 4;
    command_header->prdtl = 1; // одна PRDT запись
    command_header->write = 0;

    port->ci |= (1 << cmd_num); // отправляем нашу команду

    while (port->ci & (1 << cmd_num))
        halt(); // синхронно ждем завершения

    // далее - парсинг ответа команды

    ahci_basic_identify_data_t *result = malloc(sizeof(ahci_basic_identify_data_t));
    memset(result, 0, sizeof(ahci_basic_identify_data_t));

    // полученный буффер, согласно спецификации, интерпретируется как массив из 256 2-байтовых слов
    uint16_t *answer_words = (uint16_t*)cmd_answer_buffer;
    
    // парсим строку serial
    for (int i = 0;i < 10;i++)
    {
        result->serial[i*2] = (answer_words[10 + i] >> 8) & 0xFF;
        result->serial[i*2+1] = answer_words[10 + i] & 0xFF;
    }

    // парсим строку model
    for (int i = 0;i < 20;i++)
    {
        result->model[i*2] = (answer_words[27 + i] >> 8) & 0xFF;
        result->model[i*2+1] = answer_words[27 + i] & 0xFF;
    }
    
    // парсим количество секторов

    // для LBA48
    result->sectors = (uint64_t)answer_words[100] |
                       ((uint64_t)answer_words[101] << 16) |
                       ((uint64_t)answer_words[102] << 32) |
                       ((uint64_t)answer_words[103] << 48);

    // для LBA28
    if (result->sectors == 0)
        result->sectors = (uint64_t)answer_words[60] | ((uint64_t)answer_words[61] << 16);
    
    result->lba48_supported = (answer_words[83] & (1 << 10));
    result->ncq_supported = (answer_words[76] & (1 << 8));
    result->dma_supported = (answer_words[49] & (1 << 8));

    result->port_num = port_num;
    result->port_sig = port->sig; // чтобы знать

    free(cmd_answer_buffer);

    return result;
}

dynamic_array_t *ahci_enumerate_ports()
{
    dynamic_array_t *result = dynamic_array_create(sizeof(ahci_basic_identify_data_t));

    for (byte_t i = 0;i < 32;i++)
    {
        if (!(hba->pi & (1 << i))) // проверяем, реализован ли i-й порт
            continue;
        if ((hba->ports[i].ssts & 0x0000000F) != 0x3) // проверяем, готов ли он
            continue;
        switch (hba->ports[i].sig) // проверяем, допустимое ли устройство подключено по порту
        {
            case SATA_SIG_ATA:
            case SATA_SIG_ATAPI:
            case SATA_SIG_SEMB:
            case SATA_SIG_PM:
                break;
            default:
                continue;
        }

        ahci_basic_identify_data_t *identify_data = ahci_identify(i);
        dynamic_array_push_back(result, identify_data);
        free(identify_data);
    }

    return result;
}