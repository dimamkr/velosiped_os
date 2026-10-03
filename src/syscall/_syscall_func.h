#ifndef _SYSCALL_FUNC
#define _SYSCALL_FUNC

#include "types.h"

// Таблица системных вызовов (номера из Linux)
#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_EXIT 4
#define SYS_BRK 12
#define SYS_SLEEP 162 // Linux i386 nanosleep  TODO у нас ожидание в милисекундах (не совместимо)
#define SYS_POLL 168  // Linux i386 poll

// не совместимые с Linux
#define SYS_WIN_CREATE 200
#define SYS_WIN_COMMIT 201
#define SYS_WIN_MOVE 202
#define SYS_WIN_SET_Z 203
#define SYS_WIN_DESTROY 204

#define SYSCALL6_TABLE_COUNT 256

typedef int (*syscall_fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

extern syscall_fn syscall6_table[];

#endif