#include "input_manager.h"
#include "keyboard.h"
#include "mouse.h"
#include "task.h"
#include "timer.h"

input_channel_t input_channels[INPUT_CHANNELS] = {0};

static uint32_t _input_channels_sizeof_element[INPUT_CHANNELS] =
    {
        [INPUT_CHANNEL_KEYBOARD] = sizeof(keyboard_event_t),
        [INPUT_CHANNEL_MOUSE] = sizeof(mouse_event_t)};

static inline int _find_free_stream(uint32_t ch)
{
    for (int i = 0; i < INPUT_STREAMS; ++i)
    {
        if (input_channels[ch].input_streams[i].flags & IS_FLAG_ACTIVE)
            continue;
        return i;
    }
    return -1;
}

static inline void _input_stream_destroy(input_stream_t *this)
{
    if (this->input_queue)
    {
        ring_destroy(this->input_queue);
        this->input_queue = NULL;
    }
    if (this->event)
    {
        task_event_destroy(this->event);
        this->event = NULL;
    }

    this->flags = 0;
    this->ref_count = 0;
}

static inline void _input_stream_init_default(uint32_t ch, uint32_t sid)
{
    input_stream_t *stream = &input_channels[ch].input_streams[sid];

    if (stream->flags & IS_FLAG_ACTIVE)
    {
        _input_stream_destroy(stream);
    }

    stream->input_queue = ring_create(input_channels[ch].sizeof_input_element, INPUT_QUEUE_SIZE);
    stream->event = task_event_create();
    stream->ref_count = 0;
    stream->flags = IS_FLAG_ACTIVE;
}

static inline uint32_t _input_stream_new(uint32_t ch)
{
    int sid = _find_free_stream(ch);
    if (sid < 0)
        return sid;
    _input_stream_init_default(ch, sid);
    return sid;
}

static inline input_stream_t *_input_stream_get_or_create(uint32_t ch, uint32_t sid)
{
    if (!(input_channels[ch].input_streams[sid].flags & IS_FLAG_ACTIVE))
    {
        _input_stream_init_default(ch, sid);
    }

    return &input_channels[ch].input_streams[sid];
}

void input_manager_init(void)
{
    for (uint32_t i = 0; i < INPUT_CHANNELS; ++i)
    {
        input_channels[i].sizeof_input_element = _input_channels_sizeof_element[i];

        input_channels[i].input_streams = (input_stream_t *)malloc(INPUT_STREAMS * sizeof(input_stream_t));
        memset(input_channels[i].input_streams, 0, INPUT_STREAMS * sizeof(input_stream_t));
    }
}

uint32_t input_manager_register_to_new_stream(uint32_t ch, uint32_t pid)
{
    TASK_LOCKED_FUNCTION;

    int sid = _find_free_stream(ch);
    if (sid < 0)
        return (uint32_t)-1;

    _input_stream_init_default(ch, sid);
    input_manager_register_to_stream(ch, sid, pid);
    return (uint32_t)sid;
}

void input_manager_register_to_stream(uint32_t ch, uint32_t sid, uint32_t pid)
{
    TASK_LOCKED_FUNCTION;

    input_stream_t *is = _input_stream_get_or_create(ch, sid);

    is->ref_count++;
}

void input_manager_unregister_from_stream(uint32_t ch, uint32_t sid, uint32_t pid)
{
    TASK_LOCKED_FUNCTION;

    input_stream_t *is = &input_channels[ch].input_streams[sid];

    if (!(is->flags & IS_FLAG_ACTIVE))
        return;

    is->ref_count--;
    task_event_erase(is->event, pid);

    if (is->ref_count == 0)
    {
        is->flags &= ~IS_FLAG_ACTIVE;
        _input_stream_destroy(is);
    }
}

// рассылка ввода по всем активным каналам
void input_manager_add_input_el(uint32_t ch, void *input_el)
{
    TASK_LOCKED_FUNCTION;

    static uint32_t last_warn_time = 0;
    static uint32_t dropped_count = 0;
    const uint32_t min_warn_period = 1000;

    for (uint32_t i = 0; i < INPUT_STREAMS; ++i)
    {
        input_stream_t *is = &input_channels[ch].input_streams[i];
        if (is->flags & IS_FLAG_ACTIVE)
        {
            if (unlikely(!ring_push_back(is->input_queue, input_el)))
            {
                dropped_count++;
                uint32_t now = timer_get_time();
                if (now - last_warn_time > min_warn_period)
                {
                    WARNING("input ring full ch=%d str=%d, dropped %u total", ch, i, dropped_count);
                    last_warn_time = now;
                    dropped_count = 0;
                }
            }
            task_event_flush(is->event);
        }
    }
}

// получить task_event ввода
task_event_t *input_manager_get_event(uint32_t ch, uint32_t sid)
{
    input_stream_t *is = &input_channels[ch].input_streams[sid];
    if (is->flags & IS_FLAG_ACTIVE)
    {
        return is->event;
    }
    return NULL;
}

// получить элемент ввода
bool_t input_manager_get_input_el(uint32_t ch, uint32_t sid, void *out)
{
    if (!out)
        return false;

    input_stream_t *is = &input_channels[ch].input_streams[sid];
    if (is->flags & IS_FLAG_ACTIVE)
    {

        if (!ring_pop_front(is->input_queue, out))
            return false;

        return true;
    }

    return false;
}

bool_t input_manager_queue_empty(uint32_t ch, uint32_t sid)
{
    if (sid >= INPUT_STREAMS)
        return true;

    input_stream_t *is = &input_channels[ch].input_streams[sid];
    if (!(is->flags & IS_FLAG_ACTIVE) || !is->input_queue)
        return true;

    return ring_empty(is->input_queue);
}