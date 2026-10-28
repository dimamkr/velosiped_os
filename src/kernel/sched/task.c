#include "task.h"
#include "heap.h"
#include "timer.h"
#include "system.h"
#include "konsole.h"
#include "vmm.h"
#include "elf.h"
#include "bitmap.h"
#include "int_worker.h"
#include "tss.h"
#include "sysenter.h"
#include "sched.h"

#define _STACK_TOP(task) (((uint32_t)task->stack_start + task->stack_size))

// удобная регистрация новых потоков ввода
uint32_t task_sids_none[INPUT_CHANNELS];
uint32_t task_sids_keyboard_only[INPUT_CHANNELS];
uint32_t task_sids_keyboard_mouse_only[INPUT_CHANNELS];

task_t tasks[MAX_TASKS];
bitmap_t *tasks_used_bitmap;
uint32_t tasks_pr_id;
static uint32_t task_count;

task_t *current_task = NULL;
task_t *kernel_task = NULL;
task_t *lazy_task = NULL;

static bool skipped_tasks_tick = false;
static uint32_t locked = 0;
volatile uint32_t need_reschedule;

task_t *to_destroy_accumulator = NULL;

extern void task_switch(task_t *prev, task_t *next);
extern void task_switch_from_isr(void);
extern void goto_current_task(void);

#pragma GCC optimize("no-optimize-sibling-calls")

void lazy_task_do(void *_)
{
    (void)_;

    while (1)
    {
        halt();
    }
}

static inline uint32_t _tasks_get_free_id()
{
    tasks_pr_id = bitmap_alloc_interval(tasks_used_bitmap, tasks_pr_id, 1);
    if (likely(tasks_pr_id != tasks_used_bitmap->bits_count))
    {
        return tasks_pr_id;
    }

    tasks_pr_id = bitmap_alloc_interval(tasks_used_bitmap, 0, 1);
    ASSERT(tasks_pr_id != tasks_used_bitmap->bits_count);
    return tasks_pr_id;
}

static inline void _tasks_erase(uint32_t pid)
{
    bitmap_clear_bit(tasks_used_bitmap, pid);
}

void tasks_erase_task(task_t *task)
{
    _tasks_erase(task->pid);
}

void tasks_start()
{
    goto_current_task();
}

// Обновление глобальных указателей
static inline void task_set_current(task_t *task)
{
    current_task = task;
}

static inline task_t *_task_init_prefix(uint32_t stack_size, page_dict_t *page_dict, uint32_t *input_sids)
{
    ASSERT(task_count < MAX_TASKS);

    uint32_t pid = _tasks_get_free_id();
    task_t *task = &tasks[pid];
    task->pid = pid;
    sched_task_set_state(task, TASK_READY);

    ++task_count;

    // ВАЖНО РАЗМЕР СТЕКА ВЫРОВНЕН ПО STACK_ALIGN
    task->stack_start = alligned_malloc(stack_size, STACK_ALIGN);
    task->stack_size = stack_size;

    task->page_dict = page_dict;

    task->ebp = 0;

    for (int ch = 0; ch < INPUT_CHANNELS; ++ch)
    {
        switch (input_sids[ch])
        {
        case TASK_INPUT_NONE:
            task->input_sids[ch] = TASK_INPUT_NONE;
            break;
        case TASK_INPUT_NEW:
        {
            uint32_t sid = input_manager_register_to_new_stream(ch, pid);
            if (sid == (uint32_t)-1)
            {
                WARNING("bad sid");
                task->input_sids[ch] = TASK_INPUT_NONE;
            }
            else
            {
                task->input_sids[ch] = sid;
            }
            break;
        }
        default:
            task->input_sids[ch] = input_sids[ch];
            input_manager_register_to_stream(ch, input_sids[ch], pid);
        }
    }

    // инициализация контекста fpu
    task->fpu_context = fpu_context_create();

    return task;
}

static inline task_t *_task_init_kernel(void (*entry)(void *), void *arg, uint32_t stack_size, uint32_t *input_sids)
{
    task_t *task = _task_init_prefix(stack_size, kernel_page_dict, input_sids);

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

// пользовательский стек уже выделен и в нем уже лежат аргументы
static inline task_t *_task_init_user(uint32_t user_entry, uint32_t kernel_stack_size,
                                      page_dict_t *page_dict, uint32_t user_stack_top, uint32_t *input_sids, uint32_t brk_start)
{
    task_t *task = _task_init_prefix(kernel_stack_size, page_dict, input_sids);

    // инициализация пользовательской кучи
    task->brk = brk_start;
    task->brk_end = brk_start;
    task->_task_mmap_next = 0;

    uint32_t *sp = (uint32_t *)_STACK_TOP(task);

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
void tasks_init(void (*k_entry)(void *), void *arg, uint32_t stack_size)
{
    for (int i = 0; i < INPUT_CHANNELS; ++i)
    {
        task_sids_none[i] = TASK_INPUT_NONE;
        task_sids_keyboard_mouse_only[i] = TASK_INPUT_NONE;
        task_sids_keyboard_only[i] = TASK_INPUT_NONE;
    }
    task_sids_keyboard_mouse_only[INPUT_CHANNEL_MOUSE] = TASK_INPUT_NEW;
    task_sids_keyboard_mouse_only[INPUT_CHANNEL_KEYBOARD] = TASK_INPUT_NEW;
    task_sids_keyboard_only[INPUT_CHANNEL_KEYBOARD] = TASK_INPUT_NEW;

    tasks_used_bitmap = bitmap_create(MAX_TASKS);

    // ВНИМАНИЕ ЛЕНИВАЯ ЗАДАЧА ИМЕЕТ НОМЕР СТРОГО 0
    task_t *lazy = _task_init_kernel(lazy_task_do, NULL, STACK_SIZE_LARGE, task_sids_none);
    sched_task_add_first(lazy);
    task_set_current(lazy);

    lazy_task = lazy;

    task_create_kthread(k_entry, arg, stack_size, task_sids_keyboard_only);
    kernel_task = &tasks[1];       // ВНИМАНИЕ ЗАДАЧА ЯДРА ИМЕЕТ НОМЕР СТРОГО 1
    task_set_current(kernel_task); // задача ядра

    // ВНИМАНИЕ ЗАДАЧА int_worker ИМЕЕТ НОМЕР СТРОГО 2
    int_worker_init();
}

// создание потока ядра
void task_create_kthread(void (*entry)(void *), void *arg, uint32_t stack_size, uint32_t *input_sids)
{
    task_t *task = _task_init_kernel(entry, arg, stack_size, input_sids);
    sched_task_add_new(task);
}

static void _task_create_user_process(uint32_t user_entry, uint32_t kernel_stack_size,
                                      page_dict_t *page_dict, uint32_t user_stack_top, uint32_t *input_sids, uint32_t brk_start)
{
    task_t *task = _task_init_user(user_entry, kernel_stack_size, page_dict, user_stack_top, input_sids, brk_start);
    sched_task_add_new(task);
}

// тут ввод
bool_t task_create_user_process_from_elf(void *elf_data, int argc, char **argv, uint32_t kernel_stack_size,
                                         uint32_t user_stack_size, uint32_t *input_sids)
{
    uint32_t entry;
    uint32_t end;
    page_dict_t *page_dict;
    if (!elf_user_load(elf_data, &entry, &end, &page_dict))
    {
        return false;
    }

    uint32_t user_stack_top = vmm_user_stack_create(page_dict, argc, argv, user_stack_size);

    // начало кучи выровнено на 8 (требование picolibc)
    _task_create_user_process(entry, kernel_stack_size, page_dict, user_stack_top, input_sids, (end + 7) & ~(8));
    return true;
}

// Подготовка переключения
static inline void task_switch_prepare_state(task_t *prev, task_t *next)
{
    if (sched_task_get_state(prev) == TASK_RUNNING)
    {
        sched_task_set_state(prev, TASK_READY);
    }
    sched_task_set_state(next, TASK_RUNNING);
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
    task_t *acc = to_destroy_accumulator;

    // TODO несогласованность переключения страниц со стеками в общем случае
    vmm_page_dict_switch(acc->page_dict, current_task->page_dict);

    if (acc->page_dict != kernel_page_dict)
    {
        page_dict_destroy(acc->page_dict);
    }

    free(acc->stack_start);

    for (int ch = 0; ch < INPUT_CHANNELS; ++ch)
    {
        task_input_unregister(acc, ch);
    }

    free(acc->fpu_context);

    _tasks_erase(acc->pid);
    task_count--;

    sched_task_erase(acc);

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
    sched_task_set_state(to_destroy_accumulator, TASK_TERMINATED);

    task_t *next = sched_task_get_next(current_task);
    tss_entry.esp0 = _STACK_TOP(next);

    task_set_current(next);
    sched_task_set_state(next, TASK_RUNNING);
    goto_current_task();
}

void task_switch_prepare()
{
    task_t *next = sched_task_get_next(current_task);

    // страницы ядра точно выделены
    vmm_page_dict_switch(current_task->page_dict, next->page_dict);

    // для того чтобы процессор переходил на соотв ядерный стек процесса при прерываниях (если процесс пользовательский)
    tss_entry.esp0 = _STACK_TOP(next);

    // TODO ленивое переключение через исключение
    fpu_context_save(current_task->fpu_context);

    task_switch_prepare_state(current_task, next);
    task_set_current(next);
}

void task_switch_finish()
{
    fpu_context_restore(current_task->fpu_context);
}

// Блокировка задачи
void task_sleep(uint32_t time_milisec)
{
    sched_task_set_state(current_task, TASK_SLEEPING);
    current_task->sched.activation_time = timer_get_time() + time_milisec;

    task_yield();
}

void task_lock()
{
    locked++;
}

void task_unlock()
{
    locked--;
    if (locked == 0 && skipped_tasks_tick)
    {
        task_yield();
    }
}

//  Вызывается из прерывания таймера
void tasks_tick(uint32_t time_milisec)
{
    static uint32_t time_prev_activated = 0;

    if ((time_milisec - time_prev_activated) * TASK_AUTO_SWITCH_FREQ / 1000 == 0)
    {
        return;
    }

    time_prev_activated = time_milisec;

    if (locked)
    {
        skipped_tasks_tick = true;
        return;
    }

    task_t *next = sched_task_get_next(current_task);
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

void task_wait_for_input(uint32_t channel)
{
    uint32_t sid = current_task->input_sids[channel];
    if (sid == TASK_INPUT_NONE || sid == TASK_INPUT_NEW)
        return;

    task_event_t *ev = input_manager_get_event(channel, sid);
    if (ev)
        task_wait_until(ev);
}

bool_t task_input_get(uint32_t channel, void *out)
{
    uint32_t sid = current_task->input_sids[channel];
    if (sid == TASK_INPUT_NONE || sid == TASK_INPUT_NEW)
    {
        WARNING("bas sid: %u", current_task->input_sids[channel]);
        return false;
    }
    return input_manager_get_input_el(channel, sid, out);
}

bool_t task_input_queue_empty(uint32_t channel)
{
    uint32_t sid = current_task->input_sids[channel];
    if (sid == TASK_INPUT_NONE || sid == TASK_INPUT_NEW)
    {
        WARNING("bas sid: %u", current_task->input_sids[channel]);
        return true;
    }
    return input_manager_queue_empty(channel, sid);
}

void task_input_unregister(task_t *task, uint32_t channel)
{
    uint32_t sid = task->input_sids[channel];
    if (sid == TASK_INPUT_NONE || sid == TASK_INPUT_NEW)
        return;
    input_manager_unregister_from_stream(channel, sid, task->pid);
}

void task_set_state_waiting(uint32_t pid)
{
    sched_task_set_state(&tasks[pid], TASK_WAITING);
}
void task_set_state_ready(uint32_t pid)
{
    sched_task_set_state(&tasks[pid], TASK_READY);
}