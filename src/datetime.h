#ifndef DATETIME
#define DATETIME

#include "types.h"

void system_get_datetime(uint16_t *year, uint8_t *month, uint8_t *day,
                         uint8_t *hour, uint8_t *minute, uint8_t *second);

#endif