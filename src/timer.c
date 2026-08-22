#include "timer.h"
#include "konsole.h"
#include "task.h"

// иначе компилятор может заменить call и ret на jump + leave, и это сломает стек, поскольку он управляется полностью вручную на ассемблере
#pragma GCC optimize("no-optimize-sibling-calls")

static volatile uint32_t timer_ticks_count = 0;

void timer_callback(isr_data_t data)
{
    timer_ticks_count++;

    scheduler_tick(timer_get_time());
}

static uint32_t timer_frequency;
static const uint32_t timer_qartz_frequency = 1193180;

// частота прерываний в ГЦ
void timer_init(uint32_t frequency)
{
    timer_frequency = frequency;

    uint32_t raw_period = timer_qartz_frequency / frequency;

    if (raw_period < 1 || raw_period > 0xFFFF)
    {
        konsole_printf("\n%d\n", raw_period);
        PANIC("BAD TIMER PERIOD");
    }

    uint16_t period = (uint16_t)raw_period;

    outb(TIMER0, 0b00110110); // настройки таймера

    outb(TIMER0_DATA, period);
    outb(TIMER0_DATA, period >> 8);

    interrupt_register(IRQ0, timer_callback);
}

// в милисекундах
uint32_t timer_get_time()
{
    return 1000 * timer_ticks_count / timer_frequency;
}

uint32_t timer_get_ticks()
{
    return timer_ticks_count;
}

void timer_wait(uint32_t miliseconds)
{
    uint32_t curr_time = timer_get_time();
    while (timer_get_time() < curr_time + miliseconds)
    {
        halt();
    }
}