#ifndef SYNCAPI
#define SYNCAPI

#include "types.h"

typedef struct {
    bool_t destroyed;
    uint32_t count;
    uint32_t max_count;
    uint32_t owners;
} semaphore_t;

semaphore_t *semaphore_create(uint32_t max_count);
bool_t semaphore_acquire(semaphore_t *semaphore, uint32_t count);
void semaphore_release(semaphore_t *semaphore, uint32_t count);
void semaphore_destroy(semaphore_t *semaphore);

#endif