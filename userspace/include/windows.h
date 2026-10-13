#include "syscall.h"

__attribute__((always_inline)) static inline int sys_win_create(int w, int h, int x, int y, int z)
{
    return _sysenter6(SYS_WIN_CREATE, w, h, x, y, z, 0);
}

__attribute__((always_inline)) static inline int sys_win_commit(int handle, const void *usr_buf)
{
    return _sysenter6(SYS_WIN_COMMIT, handle, (int)usr_buf, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_win_move(int handle, int x, int y)
{
    return _sysenter6(SYS_WIN_MOVE, handle, x, y, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_win_set_z(int handle, int z)
{
    return _sysenter6(SYS_WIN_SET_Z, handle, z, 0, 0, 0, 0);
}

__attribute__((always_inline)) static inline int sys_win_destroy(int handle)
{
    return _sysenter6(SYS_WIN_DESTROY, handle, 0, 0, 0, 0, 0);
}