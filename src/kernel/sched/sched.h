#include "list.h"
#include "task.h"

// занимается только поиском следующей задачи

#include "_sched_struct.h"

void sched_task_set_state(task_t *task, task_state_t s);

task_state_t sched_task_get_state(task_t *task);

task_t *sched_task_get_next(task_t *curr_task);

void sched_task_erase(task_t *task);

void sched_task_add_new(task_t *task);
void sched_task_add_first(task_t *task);