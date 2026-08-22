#ifndef TASK_H
#define TASK_H

#include "types.h"
#include "heap.h"
#include "timer.h"
#include "system.h"
#include "konsole.h"
#include "linked_list.h"

#define MAX_TASKS 32
#define STACK_SIZE_SMALL KB
#define STACK_SIZE_LARGE MB
#define TASK_AUTO_SWITCH_FREQ 100

typedef enum
{
    TASK_RUNNING,
    TASK_READY,
    TASK_TERMINATED,
    TASK_SLEEPING
} task_state_t;

typedef struct
{
    uint32_t pid;

    uint32_t esp; // указатель стека (сохраняется при переключении)
    uint32_t ebp;
    uint32_t eip; // точка входа

    uint32_t *stack_start; // выделенный стек
    uint32_t stack_size;

    task_state_t state;
    uint32_t activation_time;

    linked_list_node_t *node;
} __attribute__((packed)) task_t;

void goto_current_task(void);

void scheduler_start(void);
void scheduler_init(void (*kernel_task_entry)(void), uint32_t stack_size);
void scheduler_tick(uint32_t time_milisec); // вызывается из прерывания таймера

void task_create(void (*entry)(void), uint32_t stack_size);
void task_yield(void);
void task_exit(void);
void task_sleep(uint32_t ticks);
void task_lock(void);
void task_unlock(void);
task_t *task_get_next(void);
void task_set_current(task_t *task);
void task_switch_prepare(task_t *prev, task_t *next);

extern task_t *current_task;
extern volatile uint32_t need_reschedule;

#endif