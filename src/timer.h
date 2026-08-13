#ifndef TIMER
#define TIMER

#include "types.h"
#include "isr.h"
#include "system.h"
#include "pic.h"

#define TIMER0 0x43
#define TIMER0_DATA 0x40

void timer_callback(isr_data_t data);
void timer_init(uint32_t);
uint32_t timer_get_ticks();
uint32_t timer_get_time();

#endif