#include "sysenter.h"
#include "heap.h"
#include "task.h"
#include "konsole.h"

static void *_sysenter_stack_start;

static int sys_exit(uint32_t status)
{
    task_exit();

    return -1;
}

static int sys_write(uint32_t file_descriptor, const char *buff, uint32_t size)
{
    // konsole_print(buff); проблемка

    return size;
}

uint32_t sysenter_stack_create_and_map(page_dict_t *page_dict)
{
    page_dict_map_interval(page_dict, SYSENTER_STACK_START_VADDR, SYSENTER_STACK_SIZE, PAGE_PRESENT | PAGE_RW);
}

int sysenter_dispatcher(uint32_t syscall_num, const uint32_t *user_stack)
{
    switch (syscall_num)
    {
        case SYS_EXIT:
            return sys_exit(user_stack[0]);

        case SYS_WRITE:
            return sys_write(user_stack[0], (void*)user_stack[1], user_stack[2]);
    }
}

void sysenter_init()
{
    uint32_t sysenter_handler_phys = vmm_vaddr_to_phys(sysenter_handler_entry);

    // page_dict_unmap_page(kernel_page_dict, ((uint32_t)_trampoline_stack_start & 0xFFFFF000));
    // page_dict_unmap_page(kernel_page_dict, ((uint32_t)sysenter_dispatcher & 0xFFFFF000));

    page_dict_map_page_to_phys(kernel_page_dict, SYSENTER_HANDLER_VADDR, sysenter_handler_phys, PAGE_PRESENT | PAGE_USER);

    wrmsr(MSR_IA32_SYSENTER_CS, SYSENTER_CS, 0);
    wrmsr(MSR_IA32_SYSENTER_EIP, SYSENTER_HANDLER_VADDR, 0);
    wrmsr(MSR_IA32_SYSENTER_ESP, SYSENTER_STACK_VADDR, 0);
}