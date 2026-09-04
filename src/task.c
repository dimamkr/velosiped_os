#include "task.h"
#include "heap.h"
#include "timer.h"
#include "system.h"
#include "konsole.h"
#include "linked_list.h"
#include "vmm.h"

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

extern page_dict_t *kernel_page_dict;

#pragma GCC optimize("no-optimize-sibling-calls")

void lazy_task(void *_)
{
    (void)_;

    while (1)
    {
        halt();
    }
}

void scheduler_start()
{
    goto_current_task();
}

// ВАЖНО РАЗМЕР СТЕКА ВЫРОВНЕН ПО STACK_ALIGN
static task_t *task_init_default(void (*entry)(void *), void *arg, uint32_t stack_size)
{
    task_t *task = &tasks[task_count];
    task->pid = task_count;
    task->state = TASK_READY;
    ++task_count;

    // стек должен быть выровнен
    task->stack_start = alligned_malloc(stack_size, STACK_ALIGN);
    task->stack_size = stack_size;

    uint32_t *sp = (uint32_t *)((uint32_t)task->stack_start + stack_size);

    // арумент
    *--sp = (uint32_t)arg;

    // адрес возврата (уничтожение)
    *--sp = (uint32_t)task_exit;

    // Запоминаем адрес, где лежит фиктивный адрес возврата – это будет ESP после iret
    // uint32_t esp_after_iret = (uint32_t)sp;

    // подставные данные для iret
    // ss и esp не нужны при переходе без смены привелегий (но стоит помнить об этих вещах)
    // *--sp = 0x10;            // SS
    // *--sp = esp_after_iret;  // ESP (после iret)
    *--sp = 0x202;           // EFLAGS
    *--sp = 0x08;            // CS
    *--sp = (uint32_t)entry; // EIP

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

    task->page_dict = kernel_page_dict;

    return task;
}

// Инициализация планировщика и передача управления ему
void scheduler_init(void (*k_entry)(void *), void *arg, uint32_t stack_size)
{
    task_t *lazy = task_init_default(lazy_task, NULL, STACK_SIZE_LARGE);
    lazy->node = linked_list_create_root_cycle(&lazy, sizeof(task_t *));
    task_set_current(lazy);

    task_create(k_entry, arg, stack_size);
    task_set_current(&tasks[1]); // задача ядра
}

// Создание новой задачи
void task_create(void (*entry)(void *), void *arg, uint32_t stack_size)
{
    if (task_count >= MAX_TASKS)
        PANIC("TOO MANY TASKS");

    task_t *task = task_init_default(entry, arg, stack_size);

    linked_list_node_t *node = linked_list_add(current_task_node, &task, sizeof(task_t *));
    task->node = node;
}

// задача но со своим словарем страниц
void task_create_process(void (*entry)(void *), void *arg, uint32_t stack_size, page_dict_t *page_dict)
{
    task_t *task = task_init_default(entry, arg, stack_size);
    task->page_dict = page_dict;

    linked_list_node_t *node = linked_list_add(current_task_node, &task, sizeof(task_t *));
    task->node = node;
}

static inline void process_task_state(task_t *task, uint32_t time_milisec)
{
    // для пропуска ленивой задачи
    if (task == &tasks[0])
    {
        task->state = TASK_WAITING;
        return;
    }

    switch (task->state)
    {
    case TASK_SLEEPING:
        if (time_milisec >= task->activation_time)
        {
            task->state = TASK_READY;
        }
        break;

    default:
        break;
    }
}

// Поиск следующей готовой задачи а также отложенная обработка задач
task_t *task_get_next()
{
    uint32_t time_milisec = timer_get_time();

    linked_list_node_t *node = current_task->node;

    do
    {
        node = node->right;
        // Извлекаем указатель на task_t из узла (в узле хранится task_t**)
        task_t *t = *(task_t **)node->value;

        process_task_state(t, time_milisec);
        if (t->state == TASK_READY || t->state == TASK_RUNNING)
        {
            return t;
        }

    } while (node != current_task_node);

    // ничего не делающая задача
    return &tasks[0];
}

// Обновление глобальных указателей
void task_set_current(task_t *task)
{
    current_task = task;
    current_task_node = task->node;
}

// Подготовка переключения
static inline void task_switch_prepare_state(task_t *prev, task_t *next)
{
    if (prev->state == TASK_RUNNING)
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
    if (to_destroy_accumulator->page_dict != kernel_page_dict)
    {
        // ВАЖНО ЧИСТЯТСЯ СТРАНИЦЫ ЯДРА ОЧЕНЬ ПЛОХО !!!!!!!!!!!!!
        // TODO нельзя чтобы область ядра считалась свободной
        // page_dict_destroy(to_destroy_accumulator->page_dict);
    }

    vmm_page_dict_switch(to_destroy_accumulator->page_dict, current_task->page_dict);

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

void task_switch_prepare()
{
    task_t *next = task_get_next();

    // страницы ядра точно выделены
    vmm_page_dict_switch(current_task->page_dict, next->page_dict);

    task_switch_prepare_state(current_task, next);
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

void task_wait_until(task_event_t *ev)
{
    task_event_add(ev, current_task->pid);
    task_yield();
}