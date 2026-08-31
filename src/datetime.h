#ifndef DATETIME
#define DATETIME

#include "types.h"

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} datetime_t;
    
typedef struct {
    uint16_t date;
    uint16_t time;
} datetime_fat_t;

void datetime_get(datetime_t *result);
void datetime_fat_from_datetime(const datetime_t *datetime, datetime_fat_t *fat_datetime);
void datetime_datetime_from_fat(const datetime_fat_t *fat_datetime, datetime_t *datetime);

#endif