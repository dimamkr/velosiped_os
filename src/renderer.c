#include "renderer.h"
#include "framebuffer.h"
#include "font.h"
#include "random.h"

#define fb framebuffer

void renderer_draw_char(uint8_t *font8x16, char symbol, uint32_t x_left, uint32_t y_up, uint32_t fg_color, uint32_t bg_color)
{
    const uint8_t *glyph = font8x16 + (uint8_t)symbol * FONT_GLYPH_SIZE;

    for (uint32_t y = 0; y < FONT_HEIGHT; ++y)
    {
        for (uint32_t x = 0; x < FONT_WIDTH; ++x)
        {
            uint32_t color = FONT_GET_BIT(glyph, y, x) ? fg_color : bg_color;
            framebuffer_put_pixel(x_left + x, y_up + y, color);
        }
    }
}

void renderer_flush()
{
    framebuffer_flush();
}
