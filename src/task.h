#ifndef TASK_H
#define TASK_H

#include "types.h"
#include "heap.h"
#include "task_event.h"

#define MAX_TASKS 32
#define TASK_AUTO_SWITCH_FREQ 100

#define STACK_SIZE_TINY KB / 4
#define STACK_SIZE_SMALL KB
#define STACK_SIZE_LARGE MB
#define STACK_SIZE_ENORMOUS 4 * MB

typedef enum
{
    TASK_RUNNING,
    TASK_READY,
    TASK_TERMINATED,
    TASK_SLEEPING,
    TASK_WAITING
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
void scheduler_init(void (*kernel_task_entry)(void *), void *arg, uint32_t stack_size);
void scheduler_tick(uint32_t time_milisec); // вызывается из прерывания таймера

void task_create(void (*entry)(void *), void *arg, uint32_t stack_size);
void task_yield(void);
void task_exit(void);
void task_sleep(uint32_t ticks);
void task_lock(void);
void task_unlock(void);
task_t *task_get_next(void);
void task_set_current(task_t *task);
void task_switch_prepare(task_t *prev, task_t *next);
void task_wait_until(task_event_t *ev);

extern task_t *current_task;
extern volatile uint32_t need_reschedule;

// трюк для автоматической расстановки task_lock/unlock

static inline void __task_unlock_trick()
{
    task_unlock();
}

#define TASK_LOCKED_FUNCTION                                                                  \
    do                                                                                        \
    {                                                                                         \
        uint32_t __task_lock_guard __attribute__((cleanup(__task_unlock_trick))) = 0xABACABA; \
        task_lock();                                                                          \
    } while (0);

#endif