#include "syscall.h"
#include "isr.h"
#include "_syscall_func.h"

void syscall_int_handler(isr_data_t *data)
{
    int sysno = data->eax;
    uint32_t arg1 = data->ebx;
    uint32_t arg2 = data->ecx;
    uint32_t arg3 = data->edx;

    if (sysno < SYSCALL3_TABLE_COUNT && syscall3_table[sysno])
    {
        int ret = syscall3_table[sysno](arg1, arg2, arg3);
        data->eax = ret; // возвращаемое значение
    }
    else
    {
        data->eax = -1; // неверный номер
    }
}