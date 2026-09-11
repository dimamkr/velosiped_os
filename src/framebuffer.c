#include "framebuffer.h"
#include "vmm.h"
#include "heap.h"
#include "system.h"
#include "colors.h"

#define VBE_MODE_ADDR 0x5200
#define fb framebuffer

framebuffer_t fb = {0};

bool_t framebuffer_is_ready = false;

// считывание информации о видеорежиме полученной при загрузке
void framebuffer_read_boot_info(void)
{
    vbe_mode_info_t *mode = (vbe_mode_info_t *)VBE_MODE_ADDR;

    // Проверим, что режим вообще DirectColor (MemoryModel == 6)
    // Иначе у нас нет RGB-каналов в привычном виде
    if (mode->memory_model != 6)
        hang_forever();

    fb.width = mode->x_resolution;
    fb.height = mode->y_resolution;
    fb.bpp = mode->bits_per_pixel;
    fb.lfb_phys = mode->phys_base_ptr;

    // VBE 3.0: если linear_bytes_per_scanline != 0, используем его
    if (mode->linear_bytes_per_scanline != 0)
    {
        fb.pitch = mode->linear_bytes_per_scanline;
        fb.red_pos = mode->linear_red_field_position;
        fb.red_size = mode->linear_red_mask_size;
        fb.green_pos = mode->linear_green_field_position;
        fb.green_size = mode->linear_green_mask_size;
        fb.blue_pos = mode->linear_blue_field_position;
        fb.blue_size = mode->linear_blue_mask_size;
    }
    else
    {
        fb.pitch = mode->bytes_per_scanline;
        fb.red_pos = mode->red_field_position;
        fb.red_size = mode->red_mask_size;
        fb.green_pos = mode->green_field_position;
        fb.green_size = mode->green_mask_size;
        fb.blue_pos = mode->blue_field_position;
        fb.blue_size = mode->blue_mask_size;
    }

    fb.size_bytes = fb.pitch * fb.height;

    if (fb.width == 0 || fb.height == 0 || fb.lfb_phys == 0)
        hang_forever();
}

void framebuffer_init(void)
{
    // 1. Отображаем LFB (обычно выше 3 ГБ, MMIO)
    fb.lfb_virt = (uint32_t)vmm_map_mmio(fb.lfb_phys, fb.size_bytes);

    // 2. Back buffer в обычной RAM (кэшируемая)
    fb.back_buffer = (uint32_t *)malloc(fb.size_bytes);
    memset(fb.back_buffer, 0, fb.size_bytes);

    framebuffer_is_ready = true;
}

void framebuffer_put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    ASSERT(x < fb.width && y < fb.height);

    // back_buffer — массив uint32_t, пишем по индексу (не по pitch/4!)
    fb.back_buffer[y * fb.width + x] = color;
}

void framebuffer_flush()
{
    uint32_t *lfb = (uint32_t *)fb.lfb_virt;

    if (fb.pitch == fb.width * 4)
    {
        // Плотная укладка — копируем одним куском
        memcpy(lfb, fb.back_buffer, fb.size_bytes);
    }
    else
    {
        // Построчно с учётом pitch
        for (uint32_t y = 0; y < fb.height; y++)
        {
            memcpy((uint8_t *)lfb + y * fb.pitch,
                   fb.back_buffer + y * fb.width,
                   fb.width * 4);
        }
    }
}