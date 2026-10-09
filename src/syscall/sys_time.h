#ifndef _SYSCALL_TIME
#define _SYSCALL_TIME

#include "_syscall_tools.h"

// Linux i386 struct timespec
struct k_timespec
{
    int32_t tv_sec;
    int32_t tv_nsec;
};

// Linux i386 struct timeval
struct k_timeval
{
    int32_t tv_sec;
    int32_t tv_usec;
};

// clockid
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

int sys_clock_gettime(uint32_t clk, struct k_timespec *ts, DECL_ARGS(4));
int sys_gettimeofday(struct k_timeval *tv, void *tz, DECL_ARGS(4));
int sys_nanosleep(struct k_timespec *req, struct k_timespec *rem, DECL_ARGS(4));

void sys_time_init(void);

#endif