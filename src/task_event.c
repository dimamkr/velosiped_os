#include "task_event.h"
#include "task.h"

extern task_t tasks[];

void task_event_init(task_event_t *this)
{
    this->front = NULL;
}

void task_event_add(task_event_t *this, uint32_t pid)
{
    task_lock();
    tasks[pid].state = TASK_WAITING;

    linked_list_add_begin(&(this->front), &pid, sizeof(uint32_t));
    task_unlock();
}

void task_event_flush(task_event_t *this)
{
    while (this->front)
    {
        uint32_t pid = *(uint32_t *)this->front->value;
        tasks[pid].state = TASK_READY;
        linked_list_erase(&(this->front), this->front);
    }
}
