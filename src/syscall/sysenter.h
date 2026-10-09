#ifndef SYSENTER
#define SYSENTER

#include "types.h"
#include "system.h"
#include "paging.h"
#include "vmm.h"

#define MSR_IA32_SYSENTER_CS 0x174
#define MSR_IA32_SYSENTER_ESP 0x175
#define MSR_IA32_SYSENTER_EIP 0x176

#define SYSENTER_CS 0x8
#define SYSENTER_HANDLER_VADDR 0xB00B5000

int sysenter_dispatcher(uint32_t syscall_num, const uint32_t *user_stack);
uint32_t sysenter_stack_create_and_map(page_dict_t *page_dict);
void sysenter_init();

extern void sysenter_handler_entry();

extern uint32_t kernel_page_dict_phys;
extern void *kernel_sysenter_stack;

#endif