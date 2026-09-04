#include "pmm.h"
#include "ram.h"
#include "heap.h"
#include "bitmap.h"
#include "konsole.h"

#define PAGE_SIZE 4096
#define PAGES_COUNT (TOTAL_RAM / PAGE_SIZE)

bitmap_t *pmm_bitmap;

static inline uint32_t get_page_num(uint32_t phys_addr)
{
    return phys_addr >> 12; // /4096
}

void pmm_init()
{
    pmm_bitmap = bitmap_create(PAGES_COUNT);
    bitmap_clear_interval(pmm_bitmap, 0, pmm_bitmap->bits_count);
}

// найти первую свободную страницу и получить адрес ее начала
uint32_t pmm_alloc_frame()
{
    uint32_t index = bitmap_alloc_interval(pmm_bitmap, 1);

    if (likely(index < pmm_bitmap->bits_count))
    {
        return index * PAGE_SIZE;
    }

    PANIC("BAD PMM FRAME ALLOC");
    return 0;
}

void pmm_free_frame(uint32_t phys_addr)
{
    uint32_t index = get_page_num(phys_addr);
    if (bitmap_test_bit(pmm_bitmap, index) == 0)
    {
        PANIC("BAD PMM FRAME FREE");
    }
    bitmap_clear_bit(pmm_bitmap, index);
}

void pmm_set_alloced_flag(uint32_t phys_addr)
{
    uint32_t index = get_page_num(phys_addr);
    if (unlikely(bitmap_test_bit(pmm_bitmap, index)))
    {
        PANIC("BAD PMM SET ALLOCED FLAG");
    }
    bitmap_set_bit(pmm_bitmap, index);
}