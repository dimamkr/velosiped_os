#ifndef TASK_EVENT
#define TASK_EVENT

#include "types.h"
#include "linked_list.h"

typedef struct
{
    // очередь ожидающих процессов
    linked_list_node_t *front;

} task_event_t;

void task_event_flush(task_event_t *this);
void task_event_add(task_event_t *this, uint32_t pid);
void task_event_init(task_event_t *this);
task_event_t *task_event_create(void);

#endif