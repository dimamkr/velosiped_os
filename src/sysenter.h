#ifndef SYSENTER
#define SYSENTER

#include "types.h"
#include "system.h"

#define MSR_IA32_SYSENTER_CS  0x174
#define MSR_IA32_SYSENTER_ESP 0x175
#define MSR_IA32_SYSENTER_EIP 0x176

#define SYSENTER_CS 0x8
#define SYSENTER_STACK_START_VADDR 0xB00B4000
#define SYSENTER_STACK_VADDR 0xB00B4FFC
#define SYSENTER_HANDLER_VADDR 0xB00B5000

#define SYSENTER_STACK_SIZE 4096

// Таблица системных вызовов (номера из Linux)
#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_EXIT 4
#define SYS_BRK 12

void sysenter_dispatcher(uint32_t syscall_num);
void sysenter_init();

extern void* sysenter_carefully();
extern void sysenter_handler_entry();

extern uint32_t kernel_page_dict_phys;
extern void *kernel_sysenter_stack;

#endif