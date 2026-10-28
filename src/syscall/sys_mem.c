#include "_syscall_tools.h"
#include "task.h"
#include "paging.h"
#include "ram.h"
#include "system.h"

// -----------------------------------------------------------------
// brk: нижняя граница кучи растёт от USER_HEAP_START вверх
// -----------------------------------------------------------------

int sys_brk(uint32_t new_brk, DECL_ARGS(5))
{
    UNUSD_ARGS(5)
    task_t *t = current_task;

    if (new_brk == 0)
        return t->brk;

    uint32_t new_end = (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (new_brk < t->brk || new_end > USER_HEAP_LIMIT)
        return t->brk;

    // critical
    // TODO уменьшение размера кучи

    if (new_end > t->brk_end)
    {
        // маппим страницы до округлённого вверх new_brk
        uint32_t start = t->brk_end;
        uint32_t size = new_end - start;
        // critical
        // TODO написать выделение с занулением и сделать это в vmm
        page_dict_map_interval(t->page_dict, t->brk_end, size, PAGE_USER_FLAGS);
        t->brk_end = new_end;
    }

    t->brk = new_brk;
    return t->brk;
}

// -----------------------------------------------------------------
// mmap2: только анонимный TODO: prot
// -----------------------------------------------------------------
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20

int sys_mmap2(uint32_t addr, uint32_t len, uint32_t prot, uint32_t flags,
              uint32_t fd, uint32_t offset)
{
    UNUSD_ARGS(0)
    (void)prot;
    (void)offset;

    if (!(flags & MAP_ANONYMOUS))
        return -1;
    if (fd != (uint32_t)-1 && fd != 0)
        return -1;
    if (len == 0 || len > 256 * 1024 * 1024)
        return -1;

    task_t *t = current_task;

    len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // любой доступный адрес
    if (addr == 0)
    {
        if (t->_task_mmap_next == 0)
            t->_task_mmap_next = USER_MMAP_START;
        addr = t->_task_mmap_next;
        t->_task_mmap_next += len;
    }

    if (addr < USER_MMAP_START || addr + len > USER_MMAP_LIMIT)
        return -1;

    page_dict_map_interval(current_task->page_dict, addr, len, PAGE_USER_FLAGS);
    return (int)addr;
}

int sys_munmap(uint32_t addr, uint32_t len, DECL_ARGS(4))
{
    UNUSD_ARGS(4)
    if (addr & (PAGE_SIZE - 1))
        return -1;

    len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    page_dict_unmap_interval(current_task->page_dict, addr, len);
    return 0;
}