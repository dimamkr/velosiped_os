#ifndef SCHED_STRUCT_H
#define SCHED_STRUCT_H

#include "list.h"

typedef enum
{
    TASK_TERMINATED,
    TASK_RUNNING,
    TASK_READY,
    TASK_SLEEPING,
    TASK_WAITING
} task_state_t;

typedef struct struct_sched_data
{
    task_state_t state;
    uint32_t activation_time;

    list_node_t node;
} sched_data_t;

#endif