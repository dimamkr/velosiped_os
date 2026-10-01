#include "sysenter.h"
#include "heap.h"
#include "task.h"
#include "konsole.h"
#include "_syscall_func.h"

static void *_sysenter_stack_start;
uint32_t _sysenter_system_var;

int sysenter_dispatcher(uint32_t syscall_num, const uint32_t *user_stack)
{
    if (syscall_num < SYSCALL3_TABLE_COUNT && syscall3_table[syscall_num])
    {
        return syscall3_table[syscall_num](user_stack[0], user_stack[1], user_stack[2]);
    }
    return -1;
}

void sysenter_init()
{
    uint32_t sysenter_handler_phys = vmm_vaddr_to_phys(sysenter_handler_entry);

    // page_dict_unmap_page(kernel_page_dict, ((uint32_t)_trampoline_stack_start & 0xFFFFF000));
    // page_dict_unmap_page(kernel_page_dict, ((uint32_t)sysenter_dispatcher & 0xFFFFF000));

    page_dict_map_page_to_phys(kernel_page_dict, SYSENTER_HANDLER_VADDR, sysenter_handler_phys, PAGE_PRESENT | PAGE_USER);

    wrmsr(MSR_IA32_SYSENTER_CS, SYSENTER_CS, 0);
    wrmsr(MSR_IA32_SYSENTER_EIP, SYSENTER_HANDLER_VADDR, 0);
    wrmsr(MSR_IA32_SYSENTER_ESP, 0, 0);
}