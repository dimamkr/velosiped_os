#include "syscall.h"
#include "isr.h"
#include "_syscall_func.h"

void syscall_int_handler(isr_data_t *data)
{
    int sysno = data->eax;
    uint32_t arg1 = data->ebx;
    uint32_t arg2 = data->ecx;
    uint32_t arg3 = data->edx;
    uint32_t arg4 = data->esi;
    uint32_t arg5 = data->edi;
    uint32_t arg6 = data->ebp;

    if (sysno < SYSCALL6_TABLE_COUNT && syscall6_table[sysno])
    {
        int ret = syscall6_table[sysno](arg1, arg2, arg3, arg4, arg5, arg6);
        data->eax = ret; // возвращаемое значение
    }
    else
    {
        data->eax = -1; // неверный номер
    }
}