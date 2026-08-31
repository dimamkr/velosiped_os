#include "types.h"
#include "system.h"
#include "datetime.h"

// Чтение одного байта из CMOS
static uint8_t cmos_read(uint8_t reg)
{
    outb(0x70, (reg | 0x80)); // 0x80 запрещает NMI
    return inb(0x71);
}

// Проверка, идёт ли обновление (регистр 0x0A, бит 7)
static void cmos_wait_update()
{
    while (cmos_read(0x0A) & 0x80)
    {
        // ждём, пока бит сбросится
    }
}

// Преобразование BCD в двоичный (если нужно)
static uint8_t bcd_to_bin(uint8_t bcd)
{
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

// Чтение системной даты и времени
void datetime_get(datetime_t *result)
{
    // Ждём окончания обновления
    cmos_wait_update();

    // Читаем регистры (порядок не важен, но обычно читают секунды, минуты, часы)
    uint8_t _sec = cmos_read(0x00);
    uint8_t _min = cmos_read(0x02);
    uint8_t _hrs = cmos_read(0x04);
    uint8_t _day = cmos_read(0x07);
    uint8_t _mon = cmos_read(0x08);
    uint8_t _yr = cmos_read(0x09);

    // Определяем формат (BCD или двоичный) по биту 2 регистра 0x0B
    uint8_t regB = cmos_read(0x0B);
    if (!(regB & 0x04))
    { // бит 2 = 0 → BCD
        _sec = bcd_to_bin(_sec);
        _min = bcd_to_bin(_min);
        _hrs = bcd_to_bin(_hrs);
        _day = bcd_to_bin(_day);
        _mon = bcd_to_bin(_mon);
        _yr = bcd_to_bin(_yr);
    }

    // Преобразуем год (обычно 0–99, добавляем 2000)
    result->year = 2000 + _yr;
    result->month = _mon;
    result->day = _day;
    result->hour = _hrs;
    result->minute = _min;
    result->second = _sec;
}

void datetime_fat_from_datetime(const datetime_t *datetime, datetime_fat_t *fat_datetime)
{
    fat_datetime->date = ((datetime->year - 1980) << 9) + ((uint16_t)datetime->month << 5) + (uint16_t)datetime->day;
    fat_datetime->time = ((uint16_t)datetime->hour << 11) + ((uint16_t)datetime->minute << 5) + ((uint16_t)datetime->second / 2);
}

void datetime_datetime_from_fat(const datetime_fat_t *fat_datetime, datetime_t *datetime)
{
    datetime->year = 1980 + ((fat_datetime->date >> 9) & 0x7F);
    datetime->month = (fat_datetime->date >> 5) & 0x0F;
    datetime->day = fat_datetime->date & 0x1F;
    
    datetime->hour = (fat_datetime->time >> 11) & 0x1F;
    datetime->minute = (fat_datetime->time >> 5) & 0x3F;
    datetime->second = (fat_datetime->time & 0x1F) * 2;
}