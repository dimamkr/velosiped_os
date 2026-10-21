#ifndef TASK_STRUCT_H
#define TASK_STRUCT_H

#include "types.h"
#include "paging.h"
#include "sse.h"
#include "_sched_struct.h"
#include "input_manager.h"

typedef struct struct_task
{
    // строго фиксированный порядок (ручное обращение в asm)
    //------------------------
    uint32_t pid;

    uint32_t esp; // указатель стека (сохраняется при переключении)
    uint32_t ebp;
    uint32_t eip; // точка входа

    uint32_t *stack_start; // выделенный стек ядра
    uint32_t stack_size;

    page_dict_t *page_dict;
    //------------------------

    sched_data_t sched;

    // потоки ввода
    uint32_t input_sids[INPUT_CHANNELS];

    // sse контекст
    fpu_context_t *fpu_context;

    // пользовательская куча, память
    uint32_t brk;     // нижняя граница кучи (растёт вверх)
    uint32_t brk_end; // замаплено до этого адреса
    uint32_t _task_mmap_next;
} task_t;

_Static_assert(__builtin_offsetof(task_t, esp) == 4, "esp offset");
_Static_assert(__builtin_offsetof(task_t, stack_start) == 16, "stack_start offset");
_Static_assert(__builtin_offsetof(task_t, stack_size) == 20, "stack_size offset");
_Static_assert(__builtin_offsetof(task_t, page_dict) == 24, "page_dict offset");

#endif