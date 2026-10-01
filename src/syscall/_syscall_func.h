#include "types.h"

// Таблица системных вызовов (номера из Linux)
#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_EXIT 4
#define SYS_BRK 12

#define SYSCALL3_TABLE_COUNT 256

typedef int (*syscall_fn)(uint32_t, uint32_t, uint32_t);

extern syscall_fn syscall3_table[];