#include "task_event.h"
#include "task.h"

extern task_t tasks[];

task_event_t *task_event_create()
{
    task_event_t *t = malloc(sizeof(task_event_t));
    t->front = NULL;
    return t;
}

void task_event_add(task_event_t *this, uint32_t pid)
{
    TASK_LOCKED_FUNCTION;

    tasks[pid].state = TASK_WAITING;

    linked_list_add_begin(&(this->front), &pid, sizeof(uint32_t));
}

void task_event_flush(task_event_t *this)
{
    TASK_LOCKED_FUNCTION;

    while (this->front)
    {
        uint32_t pid = *(uint32_t *)this->front->value;
        tasks[pid].state = TASK_READY;
        linked_list_erase(&(this->front), this->front);
    }
}
