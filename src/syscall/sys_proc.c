#include "sys_proc.h"
#include "task.h"
#include "ram.h"
#include "string.h"

int sys_getpid(DECL_ARGS(6))
{
    UNUSD_ARGS(6)
    return (int)current_task->pid;
}

int sys_getppid(DECL_ARGS(6))
{
    UNUSD_ARGS(6)
    return 0; // иерархии процессов пока нет
}

// TODO нужна vfs
int sys_dup(uint32_t oldfd, DECL_ARGS(5))
{
    UNUSD_ARGS(5)
    (void)oldfd;
    return -1;
}

int sys_kill(uint32_t pid, uint32_t sig, DECL_ARGS(4))
{
    UNUSD_ARGS(4)
    (void)sig;
    // Пока только self-terminate
    if (pid == current_task->pid || pid == 0)
        task_exit();
    return -1;
}

int sys_access(const char *path, uint32_t mode, DECL_ARGS(4))
{
    UNUSD_ARGS(4)
    (void)path;
    (void)mode;
    // TODO нужна vfs
    return -1;
}

int sys_ioctl(uint32_t fd, uint32_t req, void *arg, DECL_ARGS(3))
{
    UNUSD_ARGS(3)
    (void)fd;
    (void)req;
    (void)arg;
    return -1; // пока не поддерживается
}