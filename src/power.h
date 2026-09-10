#ifndef POWER
#define POWER

#include "types.h"

#define KEYBOARD_CONTROLLER_PORT 0x64
#define CHIPSET_PORT 0xCF9

#define POWER_KEYBOARD_RESET_VALUE 0xFE
#define POWER_CHIPSET_INITIATE_RESET 0x02
#define POWER_CHIPSET_RESET_AND_INITIATE 0x06

typedef void (*power_shutdown_routine_cb) (void);

void power_invoke_all_shutdown_routines();
void power_register_shutdown_routine(power_shutdown_routine_cb routine);
bool_t power_shutdown();
bool_t power_gracefully_shutdown();
bool_t power_reboot();
bool_t power_gracefully_reboot();

#endif