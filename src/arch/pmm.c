#include "pmm.h"
#include "ram.h"
#include "heap.h"
#include "bitmap.h"
#include "konsole.h"
#include "task.h"

#define PAGE_SIZE 4096
#define PAGES_COUNT (TOTAL_RAM / PAGE_SIZE)

bitmap_t *pmm_bitmap;
uint32_t *pmm_access_count; // сколько задач покрывают на эту память
uint32_t pmm_alloc_frame_start_addr;

static inline uint32_t get_page_num(uint32_t phys_addr)
{
    return phys_addr >> 12; // /4096
}

void pmm_init()
{
    pmm_bitmap = bitmap_create(PAGES_COUNT);
    bitmap_clear_interval(pmm_bitmap, 0, pmm_bitmap->bits_count);

    pmm_access_count = (uint32_t *)malloc(sizeof(uint32_t) * PAGES_COUNT);
    memset(pmm_access_count, 0, sizeof(uint32_t) * PAGES_COUNT);

    pmm_alloc_frame_start_addr = get_page_num((uint32_t)ram_kernel_to_phys((void *)KERNEL_END) + 4 * KB);
}

// найти первую свободную страницу и получить адрес ее начала
uint32_t pmm_alloc_frame()
{
    TASK_LOCKED_FUNCTION;

    uint32_t index = bitmap_alloc_interval(pmm_bitmap, pmm_alloc_frame_start_addr, 1);
    if (unlikely(index >= pmm_bitmap->bits_count))
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

    if (unlikely(bitmap_test_bit(pmm_bitmap, index) == 0))
    {
        PANIC("BAD PMM FRAME FREE");
    }
    bitmap_clear_bit(pmm_bitmap, index);

    return true;
}

void pmm_set_alloced(uint32_t phys_addr)
{
    TASK_LOCKED_FUNCTION;

    uint32_t index = get_page_num(phys_addr);
    if (unlikely(index >= pmm_bitmap->bits_count))
    {
        PANIC("BAD PMM FRAME SET ALLOCED");
    }
    pmm_access_count[index]++;
    bitmap_set_bit(pmm_bitmap, index);
}

uint32_t pmm_get_counter(uint32_t phys_addr)
{
    uint32_t index = get_page_num(phys_addr);
    if (unlikely(index >= pmm_bitmap->bits_count))
    {
        PANIC("BAD PMM GET COUNTER");
    }
    return pmm_access_count[index];
}

bool_t pmm_test_frame(uint32_t phys_addr)
{
    uint32_t index = get_page_num(phys_addr);
    if (unlikely(index >= pmm_bitmap->bits_count))
    {
        PANIC("BAD PMM TEST FRAME");
    }
    return bitmap_test_bit(pmm_bitmap, index);
}