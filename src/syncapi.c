#include "syncapi.h"
#include "task.h"


semaphore_t *semaphore_create(uint32_t max_count)
{
    semaphore_t *result = malloc(sizeof(semaphore_t));
    result->count = max_count;
    result->owners = 0;

    return result;
}

bool_t semaphore_acquire(semaphore_t *semaphore, uint32_t count)
{
    /*
     * Нам нужно сделать, чтобы если semaphore->count >= count, мы вычли, и сделать это все блокирующе
     * Для этого будем использовать CAS-цикл (Compare-And-Swap): берем переменную, подготавливаем преобразования,
     * если она за это время изменилась - идем на новый цикл.
     */

    __sync_fetch_and_add(&(semaphore->owners), 1);

    while (true)
    {
        if (semaphore->destroyed)
        {
            __sync_fetch_and_sub(&(semaphore->owners), 1);
            return false;
        }

        uint32_t old = semaphore->count;

        if (old < count)
            continue;

        uint32_t next = old - count;

        if (__sync_bool_compare_and_swap(&(semaphore->count), old, next))
            break;
    }

    return true;
}

void semaphore_release(semaphore_t *semaphore, uint32_t count)
{
    __sync_fetch_and_add(&(semaphore->count), count);
    __sync_fetch_and_sub(&(semaphore->owners), 1);
}

void semaphore_destroy(semaphore_t *semaphore)
{
    __sync_lock_test_and_set(&(semaphore->destroyed), true);

    while (semaphore->owners > 0)
        task_yield();

    free(semaphore);
}