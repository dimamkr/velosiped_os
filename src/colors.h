#ifndef COLOR
#define COLOR

#include "framebuffer.h"

#define fb framebuffer

#define SIZEOF_PIXEL sizeof(uint32_t)

// x от 1 до 65536
#define DIV255(x) ((x + 1 + (x >> 8)) >> 8)

static inline uint32_t color_to_pixel(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t color = 0;

    color |= ((uint32_t)(r >> (8 - fb.red_size))) << fb.red_pos;
    color |= ((uint32_t)(g >> (8 - fb.green_size))) << fb.green_pos;
    color |= ((uint32_t)(b >> (8 - fb.blue_size))) << fb.blue_pos;

    color |= ((uint32_t)(a >> (8 - fb.alpha_size))) << fb.alpha_pos;

    return color;
}

static inline uint32_t color_rgb_add_a(uint32_t color, uint8_t a)
{
    color |= ((uint32_t)(a >> (8 - fb.alpha_size))) << fb.alpha_pos;
}

#define RGB(r, g, b) color_to_pixel(((1u << framebuffer.alpha_size) - 1), (r), (g), (b))
#define RGBA(r, g, b, a) color_to_pixel((a), (r), (g), (b))
#define RGB_TO_RGBA(color_rgb, a) color_rgb_add_a((color_rgb), (a))

static inline uint32_t color_blend_pixels(uint32_t color_rgba, uint32_t color_rgb)
{

    uint32_t a = (color_rgba >> fb.alpha_pos) & ((1u << fb.alpha_size) - 1);

    if (a == 255)
        return color_rgba; // полностью непрозрачный
    if (a == 0)
        return color_rgb; // полностью прозрачный

    // извлечение rgb
    uint32_t sr = (color_rgba >> fb.red_pos) & ((1u << fb.red_size) - 1);
    uint32_t sg = (color_rgba >> fb.green_pos) & ((1u << fb.green_size) - 1);
    uint32_t sb = (color_rgba >> fb.blue_pos) & ((1u << fb.blue_size) - 1);

    uint32_t dr = (color_rgb >> fb.red_pos) & ((1u << fb.red_size) - 1);
    uint32_t dg = (color_rgb >> fb.green_pos) & ((1u << fb.green_size) - 1);
    uint32_t db = (color_rgb >> fb.blue_pos) & ((1u << fb.blue_size) - 1);

    // out = src * a/255 + dst * (1 - a/255)
    uint32_t r = DIV255(sr * a + dr * (255 - a));
    uint32_t g = DIV255(sg * a + dg * (255 - a));
    uint32_t b = DIV255(sb * a + db * (255 - a));

    uint32_t out = RGB(r, g, b);

    return out;
}

#undef fb

extern uint32_t color_light_gray;
extern uint32_t color_black;
extern uint32_t color_light_green;
extern uint32_t color_yellow;
extern uint32_t color_red;
extern uint32_t color_light_red;
extern uint32_t color_white;
extern uint32_t color_light_blue;
extern uint32_t color_green;
extern uint32_t color_blue;

void colors_init(void);

#endif