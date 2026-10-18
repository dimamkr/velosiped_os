#include "_syscall_tools.h"

int sys_write(uint32_t fd, const char *buff, uint32_t size, DECL_ARGS(3));
int sys_read(uint32_t fd, void *buff, uint32_t size, DECL_ARGS(3));
int sys_sleep(uint32_t ms, DECL_ARGS(5));
int sys_poll(uint32_t fd, DECL_ARGS(5));
int sys_exit(uint32_t status, DECL_ARGS(5));
int sys_win_create(uint32_t w, uint32_t h, int32_t x, int32_t y, int32_t z, DECL_ARGS(1));
int sys_win_commit(uint32_t idx, void *usr_buf, DECL_ARGS(4));
int sys_win_move(uint32_t idx, int32_t x, int32_t y, DECL_ARGS(3));
int sys_win_set_z(uint32_t idx, int32_t z, DECL_ARGS(4));
int sys_win_destroy(uint32_t idx, DECL_ARGS(5));
