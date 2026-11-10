#include "pmm.h"
#include "ram.h"
#include "heap.h"
#include "konsole.h"
#include "task.h"
#include "st_buddy.h"

// TODO хранить все в статических массивах
// TODO mcritical работа чисто с выключенными прерываниями тут, так как иначе в будущем будут ошибки

#define PAGE_SIZE 4096
#define PAGES_COUNT (TOTAL_RAM / PAGE_SIZE)

// для 4 гб (2^20 страниц)
#define DEPTH 20

st_buddy_t *st_buddy;
uint32_t *pmm_access_count; // сколько задач покрывают на эту память

static inline uint32_t get_page_num(uint32_t phys_addr)
{
    return phys_addr >> 12; // /4096
}

void pmm_init()
{
    st_buddy = st_buddy_create(DEPTH);

    pmm_access_count = (uint32_t *)malloc(sizeof(uint32_t) * PAGES_COUNT);
    memset(pmm_access_count, 0, sizeof(uint32_t) * PAGES_COUNT);

    // резервирование стартовых страниц ядра и стартовых адресов чтобы аллокатор их никогда не трогал
    size_t n = get_page_num((uint32_t)ram_kernel_to_phys((void *)KERNEL_END) + 4 * KB);
    st_buddy_set_alloced(st_buddy, 0, n);
    for (size_t i = 0; i < n; ++i)
    {
        ++pmm_access_count[i];
    }
}

// найти первую свободную страницу и получить адрес ее начала
uint32_t pmm_alloc_frame()
{
    TASK_LOCKED_FUNCTION;

    uint32_t index = st_buddy_alloc(st_buddy, 1);
    if (unlikely(index >= st_buddy_capacity(st_buddy)))
    {
        PANIC("BAD PMM FRAME ALLOC");
    }
    pmm_access_count[index]++;
    return index * PAGE_SIZE;
}

bool pmm_free_frame(uint32_t phys_addr)
{
    TASK_LOCKED_FUNCTION;

    uint32_t index = get_page_num(phys_addr);

    if (--pmm_access_count[index])
    {
        return false;
    }

    ASSERT(phys_addr >= (KERNEL_END - RAM_VIRTUAL_START));

    if (unlikely(st_buddy_is_free(st_buddy, index)))
    {
        PANIC("BAD PMM FRAME FREE");
    }
    st_buddy_free(st_buddy, index);

    return true;
}

void pmm_set_alloced(uint32_t phys_addr)
{
    TASK_LOCKED_FUNCTION;

    uint32_t index = get_page_num(phys_addr);
    if (unlikely(index >= st_buddy_capacity(st_buddy)))
    {
        PANIC("BAD PMM FRAME SET ALLOCED");
    }
    pmm_access_count[index]++;
    st_buddy_set_alloced(st_buddy, index, 1);
}

uint32_t pmm_get_counter(uint32_t phys_addr)
{
    uint32_t index = get_page_num(phys_addr);
    if (unlikely(index >= st_buddy_capacity(st_buddy)))
    {
        PANIC("BAD PMM GET COUNTER");
    }
    return pmm_access_count[index];
}

bool_t pmm_is_free_frame(uint32_t phys_addr)
{
    uint32_t index = get_page_num(phys_addr);
    if (unlikely(index >= st_buddy_capacity(st_buddy)))
    {
        PANIC("BAD PMM TEST FRAME");
    }
    return st_buddy_is_free(st_buddy, index);
}