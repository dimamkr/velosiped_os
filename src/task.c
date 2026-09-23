#include "task.h"
#include "heap.h"
#include "timer.h"
#include "system.h"
#include "konsole.h"
#include "linked_list.h"
#include "vmm.h"
#include "elf.h"
#include "bitmap.h"
#include "int_worker.h"
#include "tss.h"

#define _STACK_TOP(task) (((uint32_t)task->stack_start + task->stack_size))

task_t tasks[MAX_TASKS];
bitmap_t *tasks_used_bitmap;
uint32_t tasks_last_used_id;
static uint32_t task_count;

static linked_list_node_t *current_task_node; // хранит task_t*
static bool skipped_scheduler_tick = false;
static uint32_t locked = 0;

task_t *current_task = NULL;
task_t *kernel_task = NULL;
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

static inline uint32_t _tasks_get_free_id()
{
    tasks_last_used_id = bitmap_alloc_interval(tasks_used_bitmap, tasks_last_used_id, 1);
    if (likely(tasks_last_used_id != tasks_used_bitmap->bits_count))
    {
        return tasks_last_used_id;
    }

    tasks_last_used_id = bitmap_alloc_interval(0, tasks_last_used_id, 1);
    ASSERT(tasks_last_used_id != tasks_used_bitmap->bits_count);
    return tasks_last_used_id;
}

static inline uint32_t _tasks_erase(uint32_t pid)
{
    bitmap_clear_bit(tasks_used_bitmap, pid);
}

static inline task_t *_task_init_prefix(uint32_t stack_size, page_dict_t *page_dict)
{
    ASSERT(task_count < MAX_TASKS);

    uint32_t pid = _tasks_get_free_id();
    task_t *task = &tasks[pid];
    task->pid = pid;
    task->state = TASK_READY;

    ++task_count;

    // ВАЖНО РАЗМЕР СТЕКА ВЫРОВНЕН ПО STACK_ALIGN
    task->stack_start = alligned_malloc(stack_size, STACK_ALIGN);
    task->stack_size = stack_size;

    task->page_dict = page_dict;

    task->ebp = 0;

    return task;
}

static inline task_t *_task_init_kernel(void (*entry)(void *), void *arg, uint32_t stack_size)
{
    task_t *task = _task_init_prefix(stack_size, kernel_page_dict);

    // инициализация стека
    uint32_t *sp = (uint32_t *)_STACK_TOP(task);

    // арумент
    *--sp = (uint32_t)arg;

    // подставной адрес возврата (уничтожение)
    *--sp = (uint32_t)task_exit;

    // Запоминаем адрес, где лежит фиктивный адрес возврата – это будет ESP после iret
    // uint32_t esp_after_iret = (uint32_t)sp;

    // подставные данные для iret
    // здесь смены привелегий нет
    *--sp = 0x202;           // EFLAGS
    *--sp = 0x08;            // CS
    *--sp = (uint32_t)entry; // EIP

    *--sp = 0; // err_code
    *--sp = 0; // int_no

    // снимается вручную
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

    return task;
}

// размер пользовательского стека нигде не хранится и не используется (для польз стека должны быть заранее выделены страницы)
static inline task_t *_task_init_user(uint32_t user_entry, void *arg, uint32_t kernel_stack_size,
                                      page_dict_t *page_dict, uint32_t user_stack_top)
{
    task_t *task = _task_init_prefix(kernel_stack_size, page_dict);

    uint32_t *sp = (uint32_t *)_STACK_TOP(task);

    // 1) Аргумент и return address для entry (Ring 0 часть)
    // *--sp = (uint32_t)arg;
    *--sp = (uint32_t)task_exit; // если entry вернётся

    // подставные данные iret для Ring 3
    *--sp = 0x23;           // SS
    *--sp = user_stack_top; // ESP
    *--sp = 0x202;          // EFLAGS (IF=1)
    *--sp = 0x1B;           // CS (Ring 3 code)
    *--sp = user_entry;     // EIP

    // снимается вручную
    *--sp = 0; // err_code
    *--sp = 0; // int_no
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0; // EAX..EBX
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;    // ESP..EDI
    *--sp = 0x23; // DS (user data)

    task->esp = (uint32_t)sp;

    return task;
}

// Инициализация планировщика и передача управления ему
void scheduler_init(void (*k_entry)(void *), void *arg, uint32_t stack_size)
{
    tasks_used_bitmap = bitmap_create(MAX_TASKS);

    // ВНИМАНИЕ ЛЕНИВАЯ ЗАДАЧА ИМЕЕТ НОМЕР СТРОГО 0
    task_t *lazy = _task_init_kernel(lazy_task, NULL, STACK_SIZE_LARGE);
    lazy->page_dict = kernel_page_dict;
    lazy->node = linked_list_create_root_cycle(&lazy, sizeof(task_t *));
    task_set_current(lazy);

    task_create(k_entry, arg, stack_size);
    kernel_task = &tasks[1]; // ВНИМАНИЕ ЗАДАЧА ЯДРА ИМЕЕТ НОМЕР СТРОГО 1
    kernel_task->page_dict = kernel_page_dict;
    task_set_current(&tasks[1]); // задача ядра

    // ВНИМАНИЕ ЗАДАЧА int_worker ИМЕЕТ НОМЕР СТРОГО 2
    int_worker_init();
}

static inline void _task_create_node(task_t *task)
{
    linked_list_node_t *node = linked_list_add(current_task_node, &task, sizeof(task_t *));
    task->node = node;
}

// Создание новой задачи
void task_create(void (*entry)(void *), void *arg, uint32_t stack_size)
{
    task_t *task = _task_init_kernel(entry, arg, stack_size);
    if (current_task)
    {
        task->page_dict = current_task->page_dict;
    }

    _task_create_node(task);
}

// задача но со своим словарем страниц
void task_create_process(void (*entry)(void *), void *arg, uint32_t stack_size, page_dict_t *page_dict)
{
    task_t *task = _task_init_kernel(entry, arg, stack_size);
    _task_create_node(task);
}

void task_create_user_process(uint32_t user_entry, void *arg, uint32_t kernel_stack_size, page_dict_t *page_dict, uint32_t user_stack_top)
{
    task_t *task = _task_init_user(user_entry, arg, kernel_stack_size, page_dict, user_stack_top);
    _task_create_node(task);
}

bool_t task_create_user_process_from_elf(void *elf_data, void *arg, uint32_t kernel_stack_size, uint32_t user_stack_size)
{
    uint32_t entry;
    page_dict_t *page_dict;
    if (!elf_user_load(elf_data, &entry, &page_dict))
    {
        return false;
    }

    uint32_t user_stack_top = vmm_user_stack_create(page_dict, user_stack_size);

    task_create_user_process(entry, arg, kernel_stack_size, page_dict, user_stack_top);
    return true;
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
    case TASK_TERMINATED:
        _tasks_erase(task->pid);
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

// TODO рефакторинг для общей логики сишной части переключения
void task_destroy_from_accumulator()
{
    if (to_destroy_accumulator->page_dict != kernel_page_dict)
    {
        page_dict_destroy(to_destroy_accumulator->page_dict);
    }

    vmm_page_dict_switch(to_destroy_accumulator->page_dict, current_task->page_dict);

    tss_entry.esp0 = _STACK_TOP(current_task);

    _tasks_erase(to_destroy_accumulator->pid);
    task_count--;

    linked_list_erase(&current_task_node, to_destroy_accumulator->node);
    free(to_destroy_accumulator->stack_start);
    to_destroy_accumulator = NULL;
}

// Завершение задачи
void task_exit()
{
    // другого способа защитить критические структуры нет
    asm volatile("cli");

    if (current_task != NULL)
    {
        ASSERT(current_task->pid != 0); // освободить ленивую задачу
        ASSERT(current_task->pid != 1); // освободить задачу ядра
    }

    to_destroy_accumulator = current_task;
    to_destroy_accumulator->state = TASK_TERMINATED;
    task_set_current(task_get_next());
    goto_current_task();
}

void task_switch_prepare()
{
    task_t *next = task_get_next();

    if (next->pid == 4)
    {
        uint32_t volatile a = 0;
        a++;
    }

    // страницы ядра точно выделены
    vmm_page_dict_switch(current_task->page_dict, next->page_dict);

    // для того чтобы процессор переходил на соотв ядерный стек процесса при прерываниях (если процесс пользовательский)
    tss_entry.esp0 = _STACK_TOP(next);

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
    locked++;
}

void task_unlock()
{
    locked--;
    if (locked == 0 && skipped_scheduler_tick)
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