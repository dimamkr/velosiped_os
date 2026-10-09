#include "_syscall_func.h"
#include "sys_file.h"
#include "sys_mem.h"
#include "sys_proc.h"
#include "sys_time.h"
#include "sys_common.h"

syscall_fn syscall6_table[SYSCALL6_TABLE_COUNT] = {
    [SYS_EXIT] = (syscall_fn)sys_exit,
    [SYS_READ] = (syscall_fn)sys_read,
    [SYS_WRITE] = (syscall_fn)sys_write,
    [SYS_OPEN] = (syscall_fn)sys_open,
    [SYS_CLOSE] = (syscall_fn)sys_close,
    [SYS_LSEEK] = (syscall_fn)sys_lseek,
    [SYS_STAT] = (syscall_fn)sys_stat,
    [SYS_FSTAT] = (syscall_fn)sys_fstat,
    [SYS_UNLINK] = (syscall_fn)sys_unlink,
    [SYS_CHDIR] = (syscall_fn)sys_chdir,
    [SYS_GETCWD] = (syscall_fn)sys_getcwd,
    [SYS_BRK] = (syscall_fn)sys_brk,
    [SYS_MMAP2] = (syscall_fn)sys_mmap2,
    [SYS_MUNMAP] = (syscall_fn)sys_munmap,
    [SYS_GETPID] = (syscall_fn)sys_getpid,
    [SYS_GETPPID] = (syscall_fn)sys_getppid,
    [SYS_DUP] = (syscall_fn)sys_dup,
    [SYS_KILL] = (syscall_fn)sys_kill,
    [SYS_ACCESS] = (syscall_fn)sys_access,
    [SYS_IOCTL] = (syscall_fn)sys_ioctl,
    [SYS_CLOCK_GETTIME] = (syscall_fn)sys_clock_gettime,
    [SYS_GETTIMEOFDAY] = (syscall_fn)sys_gettimeofday,
    [SYS_NANOSLEEP] = (syscall_fn)sys_nanosleep,
    // не совместимые с Linux
    [SYS_SLEEP] = (syscall_fn)sys_sleep,
    [SYS_POLL] = (syscall_fn)sys_poll,
    [SYS_WIN_CREATE] = (syscall_fn)sys_win_create,
    [SYS_WIN_COMMIT] = (syscall_fn)sys_win_commit,
    [SYS_WIN_MOVE] = (syscall_fn)sys_win_move,
    [SYS_WIN_SET_Z] = (syscall_fn)sys_win_set_z,
    [SYS_WIN_DESTROY] = (syscall_fn)sys_win_destroy,
};
