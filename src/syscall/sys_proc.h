#include "_syscall_tools.h"

int sys_getpid(DECL_ARGS(6));
int sys_getppid(DECL_ARGS(6));
int sys_dup(uint32_t oldfd, DECL_ARGS(5));
int sys_kill(uint32_t pid, uint32_t sig, DECL_ARGS(4));
int sys_access(const char *path, uint32_t mode, DECL_ARGS(4));
int sys_ioctl(uint32_t fd, uint32_t req, void *arg, DECL_ARGS(3));