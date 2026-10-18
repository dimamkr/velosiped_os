#include "_syscall_tools.h"

int sys_brk(uint32_t new_brk, DECL_ARGS(5));
int sys_mmap2(uint32_t addr, uint32_t len, uint32_t prot, uint32_t flags,
              uint32_t fd, uint32_t offset);
int sys_munmap(uint32_t addr, uint32_t len, DECL_ARGS(5));