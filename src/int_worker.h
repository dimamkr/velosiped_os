#include "types.h"
#include "task_event.h"
#include "isr.h"

void int_worker_init(void);

void int_worker_add(isr_data_t data);

void int_worker_task(void *_);