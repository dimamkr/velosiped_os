#include "_syscall_tools.h"

// Linux i386 struct stat
struct kstat
{
    uint32_t st_dev;
    uint32_t st_ino;
    uint16_t st_mode;
    uint16_t st_nlink;
    uint16_t st_uid;
    uint16_t st_gid;
    uint32_t st_rdev;
    uint32_t st_size;
    uint32_t st_blksize;
    uint32_t st_blocks;
    uint32_t st_atime;
    uint32_t st_atime_nsec;
    uint32_t st_mtime;
    uint32_t st_mtime_nsec;
    uint32_t st_ctime;
    uint32_t st_ctime_nsec;
    uint32_t __unused4;
    uint32_t __unused5;
};

int sys_open(const char *path, uint32_t flags, uint32_t mode, DECL_ARGS(3));
int sys_close(uint32_t fd, DECL_ARGS(5));
int sys_lseek(uint32_t fd, int32_t offset, uint32_t whence, DECL_ARGS(3));
int sys_fstat(uint32_t fd, struct kstat *st, DECL_ARGS(4));
int sys_stat(const char *path, struct kstat *st, DECL_ARGS(4));
int sys_unlink(const char *path, DECL_ARGS(5));
int sys_chdir(const char *path, DECL_ARGS(5));
int sys_getcwd(char *buf, uint32_t size, DECL_ARGS(4));
