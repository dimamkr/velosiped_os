#include "framebuffer.h"
#include "vmm.h"
#include "heap.h"
#include "system.h"
#include "colors.h"
#include "task.h"

#define VBE_MODE_ADDR 0x5200
#define fb framebuffer

#define IN_SCREEN_RECT(y, x) (x >= 0 && x < fb.width && y >= 0 && y < fb.height)

framebuffer_t fb = {0};

bool_t framebuffer_is_ready = false; // готов к использованию

uint32_t fullscreen_x;
uint32_t fullscreen_y;

// TODO сверить с ос дев вики
//  считывание информации о видеорежиме полученной при загрузке
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

    // BIOS мог не заполнить rsvd-поля.
    // Для 32 bpp стандарт — ARGB8888, alpha в старшем байте.
    if (fb.alpha_size == 0 && fb.bpp == 32)
    {
        fb.alpha_size = 8;
        fb.alpha_pos = 24;
    }
    else
    {
        hang_forever();
    }

    if (fb.width == 0 || fb.height == 0 || fb.lfb_phys == 0)
        hang_forever();
}

void framebuffer_init(void)
{
    // 1. Отображаем LFB (обычно выше 3 ГБ, MMIO)
    fb.lfb_virt = (uint32_t)vmm_map_framebuffer(fb.lfb_phys, fb.size_bytes); // в 20 раз быстрее чем c vmm_map_mmio

    // 2. Back buffer в обычной RAM (кэшируемая)
    fb.back_buffer = (uint32_t *)malloc(fb.size_bytes);
    memset(fb.back_buffer, 0, fb.size_bytes);

    fullscreen_x = fb.width;
    fullscreen_y = fb.height;

    framebuffer_is_ready = true;
}

// TODO убрать ассерты и сделать рисовку лишь частей на экране
void framebuffer_put_xline(uint32_t *source, int32_t x, int32_t y, uint32_t pix_count)
{
    if (y < 0 || y >= (int32_t)fb.height || pix_count <= 0)
        return;

    if (x < 0)
    {
        // сдвиг в 0 (на -x)
        source -= x;
        pix_count += x;
        x = 0;
    }

    if (x >= (int32_t)fb.width)
        return;
    if (x + pix_count > (int32_t)fb.width)
        pix_count = fb.width - x;

    if (pix_count <= 0)
        return;

    memcpy_xl(fb.back_buffer + y * fb.width + x,
              source,
              pix_count * SIZEOF_PIXEL);
}

void framebuffer_put_pixel(int32_t x, int32_t y, uint32_t color_rgba)
{
    if (!IN_SCREEN_RECT(y, x))
        return;

    uint32_t idx = y * fb.width + x;
    uint32_t n_color = color_blend_pixels(color_rgba, fb.back_buffer[idx]);

    fb.back_buffer[idx] = n_color;
}

void framebuffer_flush()
{
    TASK_LOCKED_FUNCTION;

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

    // отправка всего что еще не отправилось из буффера для wc
    asm volatile("sfence");
}