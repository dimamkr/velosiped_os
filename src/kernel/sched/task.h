#ifndef TASK_H
#define TASK_H

// TODO рефакторинг с созданием .h файла прослойки для того чтобы все задачи делались желательно 1 способом

#include "types.h"
#include "task_event.h"
#include "paging.h"
#include "sse.h"

#define MAX_TASKS 32
#define TASK_AUTO_SWITCH_FREQ 100

#define STACK_SIZE_SMALL (KB)
#define STACK_SIZE_LARGE (MB)
#define STACK_SIZE_ENORMOUS (4 * MB)

#define LAZY_TASK_PID 0
#define KERNEL_TASK_PID 1
#define INT_WORKER_TASK_PID 2

#define TASK_INPUT_NEW -1  // новый поток ввода
#define TASK_INPUT_NONE -2 // без потока ввода

#include "_task_struct.h"

void goto_current_task(void);

void tasks_start(void);
void tasks_init(void (*kernel_task_entry)(void *), void *arg, uint32_t stack_size);
void tasks_tick(uint32_t time_milisec); // вызывается из прерывания таймера

void task_create_kthread(void (*entry)(void *), void *arg, uint32_t stack_size, uint32_t *input_sids);
void task_yield(void);
void task_exit(void);
void task_sleep(uint32_t ticks);
void task_lock(void);
void task_unlock(void);

void task_set_state_waiting(uint32_t pid);
void task_set_state_ready(uint32_t pid);

void task_wait_until(task_event_t *ev);
void task_wait_for_input(uint32_t channel);
bool_t task_input_get(uint32_t channel, void *out);
bool_t task_input_queue_empty(uint32_t channel);
void task_input_unregister(task_t *task, uint32_t channel);

bool_t task_create_user_process_from_elf(void *elf_data, int argc, char **argv, uint32_t kernel_stack_size,
                                         uint32_t user_stack_size, uint32_t *input_sids);

// extern task_t tasks[];
extern task_t *current_task;

extern uint32_t task_sids_none[INPUT_CHANNELS];
extern uint32_t task_sids_keyboard_only[INPUT_CHANNELS];
extern uint32_t task_sids_keyboard_mouse_only[INPUT_CHANNELS];

// трюк для автоматической расстановки task_lock/unlock

static inline void __task_unlock_trick(void *_)
{
    (void)_;
    task_unlock();
}

#define TASK_LOCKED_FUNCTION                                                                                      \
    __attribute__((unused)) uint32_t __task_lock_guard __attribute__((cleanup(__task_unlock_trick))) = 0xABACABA; \
    task_lock();

#endif