#ifndef REFCOUNT
#define REFCOUNT

#include "types.h"

#define RC_LOCK 0xFFFFFFFF // магическая константа, нужная для промежуточной блокировки в процессе destroy_if

#define RC_SHARED_OBJECT_HEADER rc_header_t __shared_object_header

#define RC_STRUCTURE_HEADER(x) (&((x)->__shared_object_header))
#define RC_INIT(x, destructor_cb) rc_init((x), RC_STRUCTURE_HEADER(x), (destructor_cb));
#define RC_TAKE(x) rc_take(RC_STRUCTURE_HEADER(x))
#define RC_RELEASE(x) rc_decref(RC_STRUCTURE_HEADER(x))
#define RC_DESTROY(x) rc_destroy(RC_STRUCTURE_HEADER(x))
#define RC_COUNT(x) rc_count(RC_STRUCTURE_HEADER(x))
#define RC_DESTROY_IF_LESS(x, value) rc_destroy_if_less(RC_STRUCTURE_HEADER(x), (value))

typedef void (*rc_destructor_cb) (void*);

typedef struct {
    rc_destructor_cb destructor;
    uint32_t refcount;
    void *self;
    bool_t destroyed;
} rc_header_t;

__attribute__((always_inline))
inline bool_t rc_incref(rc_header_t *header)
{
    while (true)
    {
        uint32_t old = header->refcount;

        if (old == RC_LOCK)
            return false;

        uint32_t new = old + 1;

        if (__sync_bool_compare_and_swap(&(header->refcount), old, new))
            break;
    }

    return true;
}

__attribute__((always_inline))
inline bool_t rc_decref(rc_header_t *header)
{
    if (__sync_fetch_and_sub(&(header->refcount), 1) == 1)
    {
        header->destructor(header->self);
        return false;
    }

    return true;
}

__attribute__((always_inline))
inline void rc_destroy(rc_header_t *header)
{
    header->destroyed = true;
}

__attribute__((always_inline))
inline bool_t rc_take(rc_header_t *header)
{
    if (!rc_incref(header))
        return false;
    if (header->destroyed)
    {
        rc_decref(header);
        return false;
    }

    return true;
}

__attribute__((always_inline))
inline void rc_init(void *self, rc_header_t *header, rc_destructor_cb destructor)
{
    header->destroyed = false;
    header->self = self;
    header->destructor = destructor;
    header->refcount = 0;
}

__attribute__((always_inline))
inline uint32_t rc_count(rc_header_t *header)
{
    return header->refcount;
}

__attribute__((always_inline))
inline bool_t rc_destroy_if_less(rc_header_t *header, uint32_t value)
{
    // уничтожение в случае если refcount < заданного значения

    uint32_t old;

    while (true)
    {
        old = header->refcount;

        if (old >= value)
            return false;

        if (__sync_bool_compare_and_swap(&(header->refcount), old, RC_LOCK))
            break;
    }

    rc_destroy(header);
    header->refcount = old;

    return true;
}

#endif