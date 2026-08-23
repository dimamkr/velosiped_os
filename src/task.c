#include "task.h"
#include "heap.h"
#include "timer.h"
#include "system.h"
#include "konsole.h"
#include "linked_list.h"

task_t tasks[MAX_TASKS];
static linked_list_node_t *current_task_node;
static uint32_t task_count;
static bool skipped_scheduler_tick = false;
static bool locked = false;

task_t *current_task = NULL;
volatile uint32_t need_reschedule;

task_t *to_destroy_accumulator = NULL;

extern void task_switch(task_t *prev, task_t *next);
extern void task_switch_from_isr(void);
extern void goto_current_task(void);

#pragma GCC optimize("no-optimize-sibling-calls")

void scheduler_start()
{
    goto_current_task();
}

static task_t *task_init_default(void (*entry)(void), uint32_t stack_size)
{
    task_t *task = &tasks[task_count];
    task->pid = task_count;
    task->state = TASK_READY;
    ++task_count;

    task->stack_start = malloc(stack_size);
    if (!task->stack_start)
        PANIC("Cannot allocate stack for task");
    task->stack_size = stack_size;

    uint32_t *sp = (uint32_t *)((uint32_t)task->stack_start + stack_size);

    // Тот же порядок, что и в task_create
    *--sp = 0x10;
    *--sp = (uint32_t)(task->stack_start + stack_size);
    *--sp = 0x202;
    *--sp = 0x08;
    *--sp = (uint32_t)entry;

    *--sp = 0; // err_code
    *--sp = 0; // int_no

    *--sp = 0; // EAX
    *--sp = 0; // ECX
    *--sp = 0; // EDX
    *--sp = 0; // EBX
    *--sp = 0; // ESP
    *--sp = 0; // EBP
    *--sp = 0; // ESI
    *--sp = 0; // EDI

    *--sp = 0x10; // DS

    task->esp = (uint32_t)sp;
    task->ebp = 0;

    return task;
}

// Инициализация планировщика и передача управления ему
void scheduler_init(void (*idle_entry)(void), uint32_t stack_size)
{
    task_t *task = task_init_default(idle_entry, stack_size);

    task->node = linked_list_create_root_cycle(&task, sizeof(task_t *));

    task_set_current(task);
}

// Создание новой задачи
void task_create(void (*entry)(void), uint32_t stack_size)
{
    if (task_count >= MAX_TASKS)
        PANIC("TOO MANY TASKS");

    task_t *task = task_init_default(entry, stack_size);

    linked_list_node_t *node = linked_list_add(current_task_node, &task, sizeof(task_t *));
    task->node = node;
}

// Поиск следующей готовой задачи а также отложенная обработка задач
task_t *task_get_next()
{
    uint32_t time_milisec = timer_get_time();

    linked_list_node_t *node = current_task_node;
    do
    {
        node = node->right;
        // Извлекаем указатель на task_t из узла (в узле хранится task_t**)
        task_t *t = *(task_t **)node->value;

        switch (t->state)
        {
        case TASK_READY:
            return t;
            break;
        case TASK_SLEEPING:
            if (time_milisec >= t->activation_time)
            {
                t->state = TASK_READY;
                return t;
            }
            break;
        case TASK_TERMINATED:
            break;

        default:
            break;
        }
    } while (node != current_task_node);

    return current_task;
}

// Обновление глобальных указателей
void task_set_current(task_t *task)
{
    current_task = task;
    current_task_node = task->node;
}

// Подготовка переключения
void task_switch_prepare(task_t *prev, task_t *next)
{
    if (prev->state != TASK_TERMINATED && prev->state != TASK_SLEEPING)
    {
        prev->state = TASK_READY;
    }
    next->state = TASK_RUNNING;
    task_set_current(next);
}

// Добровольная передача управления
void task_yield()
{
    // прерывание 48 - смена контекста
    asm volatile("int $0x30");
}

void task_destroy_from_accumulator()
{
    linked_list_erase(&current_task_node, to_destroy_accumulator->node);
    free(to_destroy_accumulator->stack_start);
    to_destroy_accumulator = NULL;
}

// Завершение задачи
void task_exit()
{
    task_lock();

    if (current_task->pid == 0)
    {
        PANIC("KERNEL TASK QUIT ATTEMPT!!!");
    }

    to_destroy_accumulator = current_task;
    to_destroy_accumulator->state = TASK_TERMINATED;
    task_set_current(task_get_next());
    goto_current_task();
}

// Блокировка задачи
void task_sleep(uint32_t time_milisec)
{
    current_task->state = TASK_SLEEPING;
    current_task->activation_time = timer_get_time() + time_milisec;

    task_yield();
}

void task_lock()
{
    locked = true;
}

void task_unlock()
{
    locked = false;
    if (skipped_scheduler_tick)
    {
        task_yield();
    }
}

//  Вызывается из прерывания таймера
void scheduler_tick(uint32_t time_milisec)
{
    static uint32_t time_prev_activated = 0;

    if ((time_milisec - time_prev_activated) * TASK_AUTO_SWITCH_FREQ / 1000 == 0)
    {
        return;
    }

    time_prev_activated = time_milisec;

    if (locked)
    {
        skipped_scheduler_tick = true;
        return;
    }

    task_t *next = task_get_next();
    if (next != current_task)
    {
        need_reschedule = 1;
    }
}