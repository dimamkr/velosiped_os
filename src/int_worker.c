#include "int_worker.h"
#include "task.h"
#include "ring.h"

#define RING_SIZE 4096

volatile bool_t int_worker_pending;
// TODO пока не готов dynamic_array с отключением расширения и возможностью резервировать место заранее может упасть
// при добавлении события во время обработки другого с посл realloc
ring_t *int_worker_queue; // хранит isr_data_t

// инициализация СТРОГО после планировщика но до включения прерываний
void int_worker_init(void)
{
    int_worker_pending = false;
    int_worker_queue = ring_create(sizeof(isr_data_t), RING_SIZE);

    task_create_kthread(int_worker_task, NULL, STACK_SIZE_LARGE);
}

// добавление данных прерывания в очередь для отложенной обработки
void int_worker_add(isr_data_t data)
{
    if (unlikely(!ring_push_back(int_worker_queue, &data)))
    {
        PANIC("INT WORKER QUEUE OVERFLOW");
    }
    int_worker_pending = true;
    task_set_state_ready(INT_WORKER_TASK_PID);
}

static inline void _call_curr_handler(isr_data_t data)
{
    interruption_bottom_handlers[data.int_no](data);
}

#include "konsole.h"
void int_worker_task(void *_)
{
    (void)_;

    while (1)
    {
        interrupt_disable(); // для избежания гонки и перевода в состояние TASK_WAITING если вдруг пришло прерывание на след строке
        if (!int_worker_pending)
        {
            task_set_state_waiting(INT_WORKER_TASK_PID);
            interrupt_enable();
            task_yield();
        }
        interrupt_enable();

        ASSERT(!ring_full(int_worker_queue)); // точно нет переполнения

        int_worker_pending = false;
        while (!ring_empty(int_worker_queue))
        {
            isr_data_t data;
            ring_pop_front(int_worker_queue, &data);
            _call_curr_handler(data);
        }
    }
}
