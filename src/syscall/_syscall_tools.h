#ifndef _SYSCALL_DEF
#define _SYSCALL_DEF

// инструменты для написания syscall обработчиков

#include "types.h"
#include "ram.h"

#define _CONCAT_UNFOLD(a, b) a##b
#define CONCAT(a, b) _CONCAT_UNFOLD(a, b)

#define _DECL_VAR(n) uint32_t _##n
#define _UNUSD_VAR(n) (void)_##n;

#define _DECL_CHAIN_0
#define _DECL_CHAIN_1 _DECL_VAR(1)
#define _DECL_CHAIN_2 _DECL_CHAIN_1, _DECL_VAR(2)
#define _DECL_CHAIN_3 _DECL_CHAIN_2, _DECL_VAR(3)
#define _DECL_CHAIN_4 _DECL_CHAIN_3, _DECL_VAR(4)
#define _DECL_CHAIN_5 _DECL_CHAIN_4, _DECL_VAR(5)
#define _DECL_CHAIN_6 _DECL_CHAIN_5, _DECL_VAR(6)

#define _UNUSD_CHAIN_0
#define _UNUSD_CHAIN_1 _UNUSD_VAR(1)
#define _UNUSD_CHAIN_2 _UNUSD_CHAIN_1 _UNUSD_VAR(2)
#define _UNUSD_CHAIN_3 _UNUSD_CHAIN_2 _UNUSD_VAR(3)
#define _UNUSD_CHAIN_4 _UNUSD_CHAIN_3 _UNUSD_VAR(4)
#define _UNUSD_CHAIN_5 _UNUSD_CHAIN_4 _UNUSD_VAR(5)
#define _UNUSD_CHAIN_6 _UNUSD_CHAIN_5 _UNUSD_VAR(6)

#define DECL_ARGS(n) CONCAT(_DECL_CHAIN_, n)
#define UNUSD_ARGS(n) CONCAT(_UNUSD_CHAIN_, n)

typedef int (*syscall_fn)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

// TODO нужна vfs
int _sys_read_file(uint32_t fd, void *buff, uint32_t size);
int _sys_write_file(uint32_t fd, const void *buff, uint32_t size);

static bool_t _user_ptr_ok(uint32_t ptr, uint32_t size)
{
    if (ptr == 0)
        return false;
    if (ptr >= RAM_VIRTUAL_START)
        return false;
    if (ptr + size < ptr)
        return false;
    if (ptr + size > RAM_VIRTUAL_START)
        return false;
    return true;
}

// ИНВАРИАНТ все отрицательные возвращаемые значения это ошибки

#endif