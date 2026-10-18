#include "sys_file.h"
#include "task.h"
#include "fat32.h"
#include "string.h"
#include "ram.h"
#include "vmm.h"
#include "konsole.h"

// TODO нужна vfs

int sys_open(const char *path, uint32_t flags, uint32_t mode, DECL_ARGS(3))
{
    UNUSD_ARGS(3)
    (void)mode;

    return -1;
}

int sys_close(uint32_t fd, DECL_ARGS(5))
{
    UNUSD_ARGS(5)
    return -1;
}

int sys_lseek(uint32_t fd, int32_t offset, uint32_t whence, DECL_ARGS(3))
{
    UNUSD_ARGS(3)
    return -1;
}

int sys_fstat(uint32_t fd, struct kstat *st, DECL_ARGS(4))
{
    UNUSD_ARGS(4)
    return -1;
}

int sys_stat(const char *path, struct kstat *st, DECL_ARGS(4))
{
    UNUSD_ARGS(4)
    return -1;
}

int sys_unlink(const char *path, DECL_ARGS(5))
{
    UNUSD_ARGS(5)
    return -1;
}

int sys_chdir(const char *path, DECL_ARGS(5))
{
    UNUSD_ARGS(5)
    (void)path;
    return -1;
}

int sys_getcwd(char *buf, uint32_t size, DECL_ARGS(4))
{
    UNUSD_ARGS(4)
    return -1;
}