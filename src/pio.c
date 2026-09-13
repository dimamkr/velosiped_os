#include "pio.h"
#include "task.h"
#include "timer.h"

// Далее, port_num считается так: младший бит: 0 - master, 1 - slave, старший: 0 - primary, 1 - secondary

bool_t pio_wait(uint16_t base)
{
    uint8_t status;
    uint32_t i = 0;
    for (;i < PIO_DISK_TIMEOUT;i++)
    {
        // берем регистр статуса
        for (uint8_t _ = 14;_--;)
            inb(base + PIO_REGISTER_STATUS); // задержка 400 нс))0)
        status = inb(base + PIO_REGISTER_STATUS);
        
        if (unlikely(status == 0xFF)) // нет канала
            return false;
        else if (likely(!(status & 0x80))) // диск не занят
            break;
    }

    if (status & 0x01) // ошибка
        return false;
    if (i == PIO_DISK_TIMEOUT)
        return false;
    if (status & 0x08) // данные готовы
        return true;

    return true;
}

bool_t pio_identify_sync(byte_t port_num, ata_basic_identify_data_t *result)
{
    uint16_t base = port_num & 2 ? PIO_CHANNEL_SECONDARY : PIO_CHANNEL_PRIMARY;
    uint16_t drive = port_num & 1 ? PIO_SLAVE : PIO_MASTER;

    // выставляем диск
    outb(base + PIO_REGISTER_DRIVE_HEAD, drive);

    // ждем
    pio_wait(base);

    // выставляем регистры
    outb(base + PIO_REGISTER_SECTORS_COUNT, 0);
    outb(base + PIO_REGISTER_LBA_LO, 0);
    outb(base + PIO_REGISTER_LBA_MID, 0);
    outb(base + PIO_REGISTER_LBA_HI, 0);

    // отправляем команду
    outb(base + PIO_REGISTER_COMMAND, ATA_CMD_IDENTIFY);

    if (!pio_wait(base)) // ждем завершения команды
        return false;

    uint16_t *identify_data = malloc(256 * sizeof(uint16_t));

    for (uint16_t i = 0;i < 256;i++)
        identify_data[i] = inw(base + PIO_REGISTER_DATA); // читаем данные

    ata_parse_identify_answer(identify_data, result);

    free(identify_data);

    return true;
}

bool_t pio_flush_cache_sync(byte_t port_num)
{
    uint16_t base = port_num & 2 ? PIO_CHANNEL_SECONDARY : PIO_CHANNEL_PRIMARY;
    uint16_t drive = port_num & 1 ? PIO_SLAVE : PIO_MASTER;

    // выставляем диск
    outb(base + PIO_REGISTER_DRIVE_HEAD, drive);

    // ждем
    pio_wait(base);

    // отправляем команду
    outb(base + PIO_REGISTER_COMMAND, ATA_CMD_FLUSH_CACHE);

    if (!pio_wait(base)) // ждем завершения команды
        return false;

    return true;
}

bool_t pio_transfer_sync(byte_t port_num, ata_lba_t lba, uint32_t sectors_count, void *buffer, bool_t write)
{
    uint16_t *word_buffer = buffer;

    uint16_t base = port_num & 2 ? PIO_CHANNEL_SECONDARY : PIO_CHANNEL_PRIMARY;
    uint16_t drive = port_num & 1 ? PIO_SLAVE : PIO_MASTER;

    uint32_t lba28_start = lba.lba32 & 0x0FFFFFFF; // 28-битный LBA
    uint32_t sectors_needed = sectors_count; // сколько осталось прочитать секторов

    // ждем
    pio_wait(base);

    while (sectors_needed > 0)
    {
        // выставляем диск (с старшими битами lba, режим lba28)
        outb(base + PIO_REGISTER_DRIVE_HEAD, drive | (lba28_start >> 24));

        // количество секторов за заход
        uint32_t sectors_transfered = min(sectors_needed, 255);

        // отправлям количество секторов
        outb(base + PIO_REGISTER_SECTORS_COUNT, sectors_transfered);

        // записываем LBA
        outb(base + PIO_REGISTER_LBA_LO, lba28_start);
        outb(base + PIO_REGISTER_LBA_MID, lba28_start >> 8);
        outb(base + PIO_REGISTER_LBA_HI, lba28_start >> 16);
    
        if (write)
        {
            // отправляем команду
            outb(base + PIO_REGISTER_COMMAND, ATA_CMD_WRITE_PIO);

            for (uint32_t i = 0;i < sectors_transfered;i++)
            {
                if (!pio_wait(base))
                    return false;

                for (uint32_t j = 0;j < 256;j++)
                    outw(base + PIO_REGISTER_DATA, word_buffer[sectors_count - sectors_needed + i * 256 + j]);

                for (uint16_t _ = 0;_ < 400;_++)
                    asm volatile("pause"); // задержка 
            }
        }
        else
        {
            // отправляем команду
            outb(base + PIO_REGISTER_COMMAND, ATA_CMD_READ_PIO);

            for (uint32_t i = 0;i < sectors_transfered;i++)
            {
                if (!pio_wait(base))
                    return false;

                for (uint32_t j = 0;j < 256;j++)
                    word_buffer[sectors_count - sectors_needed + i * 256 + j] = inw(base + PIO_REGISTER_DATA);
            }
        }

        sectors_needed -= sectors_transfered;
        lba28_start += sectors_transfered;
    }

    return true;
}

dynamic_array_t *pio_enumerate_ports()
{
    dynamic_array_t *result = dynamic_array_create(sizeof(ata_basic_identify_data_t));

    for (uint8_t i = 0;i < 4;i++)
    {
        ata_basic_identify_data_t port_data;

        if (!pio_identify_sync(i, &port_data))
            continue;

        if (port_data.model[0] == '\0')
            continue;

        port_data.port_sig = ATA_SIGNATURE;
        port_data.port_num = i;

        dynamic_array_push_back(result, &port_data);
    }

    return result;
}

void pio_init()
{
    // отключаем прерывание прочтения (потом надо нормально сделать)
    outb(PIO_PRIMARY_CONTROL_PORT, 0x02); 
    outb(PIO_SECONDARY_CONTROL_PORT, 0x02); 
}