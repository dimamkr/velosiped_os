#ifndef INPUT_MANAGER
#define INPUT_MANAGER

#include "types.h"
#include "ring.h"
#include "task_event.h"

#define INPUT_STREAMS 32
#define INPUT_CHANNELS 4
#define INPUT_QUEUE_SIZE 64

#define IS_FLAG_ACTIVE 1

#define INPUT_CHANNEL_KEYBOARD 0
#define INPUT_CHANNEL_MOUSE 1

typedef struct
{
    ring_t *input_queue;
    task_event_t *event;
    uint32_t ref_count;
    uint32_t flags;
} input_stream_t;

typedef struct
{
    uint32_t sizeof_input_element;
    input_stream_t *input_streams;
} input_channel_t;

void input_manager_init(void);
uint32_t input_manager_register_to_new_stream(uint32_t ch, uint32_t pid);
void input_manager_register_to_stream(uint32_t ch, uint32_t sid, uint32_t pid);
void input_manager_unregister_from_stream(uint32_t ch, uint32_t sid, uint32_t pid);
void input_manager_add_input_el(uint32_t ch, void *input_el);
bool_t input_manager_get_input_el(uint32_t ch, uint32_t sid, void *out);
bool_t input_manager_queue_empty(uint32_t ch, uint32_t sid);

task_event_t *input_manager_get_event(uint32_t ch, uint32_t sid);

#endif