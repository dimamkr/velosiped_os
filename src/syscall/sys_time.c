#include "sys_time.h"
#include "task.h"
#include "timer.h"
#include "ram.h"
#include "string.h"
#include "datetime.h"

// Сегодняшняя эпоха в секундах, зафиксированная при загрузке.
// Заполняется один раз в sys_time_init.
static uint32_t _boot_epoch_sec = 0;

void sys_time_init(void)
{
    // Прочитать RTC один раз и запомнить как wall-clock базу
    datetime_t dt;
    datetime_get(&dt);
    _boot_epoch_sec = datetime_timestamp_from_datetime(&dt);
}

int sys_clock_gettime(uint32_t clk, struct k_timespec *ts, DECL_ARGS(4))
{
    UNUSD_ARGS(4)

    if (!_user_ptr_ok((uint32_t)ts, sizeof(*ts)))
        return -1;

    uint32_t ms = timer_get_time();
    struct k_timespec tmp;

    switch (clk)
    {
    case CLOCK_REALTIME:
        // boot_epoch_sec + прошедшие секунды с загрузки
        tmp.tv_sec = _boot_epoch_sec + ms / 1000;
        tmp.tv_nsec = (ms % 1000) * 1000000;
        break;

    case CLOCK_MONOTONIC:
        tmp.tv_sec = ms / 1000;
        tmp.tv_nsec = (ms % 1000) * 1000000;
        break;

    default:
        return -1;
    }

    memcpy(ts, &tmp, sizeof(tmp));
    return 0;
}

int sys_gettimeofday(struct k_timeval *tv, void *tz, DECL_ARGS(4))
{
    UNUSD_ARGS(4)
    (void)tz;

    if (!tv)
        return 0;

    if (!_user_ptr_ok((uint32_t)tv, sizeof(*tv)))
        return -1;

    uint32_t ms = timer_get_time();
    struct k_timeval tmp;
    tmp.tv_sec = _boot_epoch_sec + ms / 1000;
    tmp.tv_usec = (ms % 1000) * 1000;
    memcpy(tv, &tmp, sizeof(tmp));
    return 0;
}

int sys_nanosleep(struct k_timespec *req, struct k_timespec *rem, DECL_ARGS(4))
{
    UNUSD_ARGS(4)

    if (!req || !_user_ptr_ok((uint32_t)req, sizeof(*req)))
        return -1;

    if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1000000000L)
        return -1;

    uint32_t ms = (uint32_t)req->tv_sec * 1000 + (uint32_t)req->tv_nsec / 1000000;

    task_sleep(ms);

    if (rem && _user_ptr_ok((uint32_t)rem, sizeof(*rem)))
    {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    return 0;
}