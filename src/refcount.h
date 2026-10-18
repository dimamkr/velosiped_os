#ifndef REFCOUNT
#define REFCOUNT

#include "types.h"

#define RC_SHARED_OBJECT_HEADER rc_header_t __shared_object_header

#define RC_STRUCTURE_HEADER(x) (&((x)->__shared_object_header))
#define RC_INIT(x, destructor_cb) rc_init((x), RC_STRUCTURE_HEADER(x), (destructor_cb));
#define RC_TAKE(x) rc_take(RC_STRUCTURE_HEADER(x))
#define RC_RELEASE(x) rc_decref(RC_STRUCTURE_HEADER(x))

typedef void (*rc_destructor_cb) (void*);

typedef struct {
    rc_destructor_cb destructor;
    uint8_t refcount;
    void *self;
    bool_t destroyed;
} rc_header_t;

__attribute__((always_inline))
inline bool_t rc_incref(rc_header_t *header)
{
    if (__sync_fetch_and_add(&(header->refcount), 1) == 0)
        return false;

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
    header->refcount = 1;
}

#endif