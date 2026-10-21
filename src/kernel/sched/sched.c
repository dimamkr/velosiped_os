#include "sched.h"
#include "list.h"
#include "timer.h"
#include "task.h"
#include "_task_tools.h"

#define sched_of(sched_ptr) container_of(sched_ptr, task_t, sched)

static inline void _proc_task_state(task_t *task, uint32_t time_milisec)
{
    // для пропуска ленивой задачи
    if (task == lazy_task)
    {
        sched_task_set_state(task, TASK_WAITING);
        return;
    }

    switch (sched_task_get_state(task))
    {
    case TASK_SLEEPING:
        if (time_milisec >= task->sched.activation_time)
        {
            sched_task_set_state(task, TASK_READY);
        }
        break;
    case TASK_TERMINATED:
        // TODO лучше тут не удалять, а использовать очередь зомби задач и потом их чистить
        tasks_erase_task(task);
        break;

    default:
        break;
    }
}

task_t *sched_task_get_next(task_t *curr_task)
{
    uint32_t time_milisec = timer_get_time();

    list_node_t *n = &curr_task->sched.node;

    do
    {
        n = n->right;
        task_t *t = sched_of(container_of(n, sched_data_t, node));

        _proc_task_state(t, time_milisec);
        if (sched_task_get_state(t) == TASK_READY || sched_task_get_state(t) == TASK_RUNNING)
        {
            return t;
        }

    } while (n != &curr_task->sched.node);

    // ничего не делающая задача
    return lazy_task;
}

void sched_task_erase(task_t *task)
{
    list_erase_cycle(&task->sched.node);
}

void sched_task_add_first(task_t *task)
{
    list_init_root_cycle(&task->sched.node);
}

void sched_task_add_new(task_t *task)
{
    list_add(current_task->sched.node.left, &task->sched.node);
}

// функция нужна на будущее когда будем переносить задачи в разные места по состояниям
void sched_task_set_state(task_t *task, task_state_t s)
{
    task->sched.state = s;
}

task_state_t sched_task_get_state(task_t *task)
{
    return task->sched.state;
}