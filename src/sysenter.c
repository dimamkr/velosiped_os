#include "sysenter.h"
#include "paging.h"
#include "vmm.h"
#include "heap.h"
#include "task.h"
#include "konsole.h"


__attribute__((aligned(4096))) static byte_t _trampoline_stack_start [4096];
static void *_trampoline_stack;

static void *_kernel_sysenter_stack_start;
void *kernel_sysenter_stack;
uint32_t kernel_page_dict_phys;

void sysenter_dispatcher(uint32_t syscall_num)
{
    konsole_println("hello world!");
}

void sysenter_init()
{
    kernel_page_dict_phys = vmm_vaddr_to_phys(kernel_page_dict->page_dir);

    _kernel_sysenter_stack_start = alligned_malloc(SYSENTER_STACK_SIZE, 4);
    kernel_sysenter_stack = _kernel_sysenter_stack_start + SYSENTER_STACK_SIZE - 4;

    uint32_t trampoline_stack_start_phys = vmm_vaddr_to_phys(_trampoline_stack_start);
    uint32_t sysenter_handler_phys = vmm_vaddr_to_phys(sysenter_handler_entry);

    // page_dict_unmap_page(kernel_page_dict, ((uint32_t)_trampoline_stack_start & 0xFFFFF000));
    // page_dict_unmap_page(kernel_page_dict, ((uint32_t)sysenter_dispatcher & 0xFFFFF000));

    page_dict_map_page_to_phys(kernel_page_dict, SYSENTER_HANDLER_VADDR, sysenter_handler_phys, PAGE_PRESENT | PAGE_USER);
    page_dict_map_page_to_phys(kernel_page_dict, SYSENTER_STACK_START_VADDR, trampoline_stack_start_phys, PAGE_PRESENT | PAGE_RW);

    wrmsr(MSR_IA32_SYSENTER_CS, SYSENTER_CS, 0);
    wrmsr(MSR_IA32_SYSENTER_EIP, SYSENTER_HANDLER_VADDR, 0);
    wrmsr(MSR_IA32_SYSENTER_ESP, SYSENTER_STACK_VADDR, 0);
}