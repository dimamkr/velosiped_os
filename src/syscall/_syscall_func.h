#ifndef _SYSCALL_FUNC
#define _SYSCALL_FUNC

#include "types.h"
#include "_syscall_tools.h"

// Linux i386 ABI (совместимы с Linux)
#define SYS_EXIT 1
#define SYS_READ 3
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6
#define SYS_UNLINK 10
#define SYS_CHDIR 12
#define SYS_LSEEK 19
#define SYS_GETPID 20
#define SYS_ACCESS 33
#define SYS_KILL 37
#define SYS_MKDIR 39
#define SYS_RMDIR 40
#define SYS_DUP 41
#define SYS_BRK 45
#define SYS_IOCTL 54
#define SYS_GETPPID 64
#define SYS_GETTIMEOFDAY 78
#define SYS_MUNMAP 91
#define SYS_STAT 106
#define SYS_FSTAT 108
#define SYS_NANOSLEEP 162
#define SYS_GETCWD 183
#define SYS_MMAP2 192
#define SYS_CLOCK_GETTIME 265

// не совместимо с Linux (доступны номера 500-511)
#define SYS_WIN_CREATE 500
#define SYS_WIN_COMMIT 501
#define SYS_WIN_MOVE 502
#define SYS_WIN_SET_Z 503
#define SYS_WIN_DESTROY 504
#define SYS_SLEEP 505
#define SYS_POLL 506

#define SYSCALL6_TABLE_COUNT 512

extern syscall_fn syscall6_table[];

#endif