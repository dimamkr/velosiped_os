#ifndef COLOR
#define COLOR

#include "framebuffer.h"

#define fb framebuffer

static inline uint32_t color_to_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t color = 0;
    color |= ((uint32_t)(r >> (8 - fb.red_size))) << fb.red_pos;
    color |= ((uint32_t)(g >> (8 - fb.green_size))) << fb.green_pos;
    color |= ((uint32_t)(b >> (8 - fb.blue_size))) << fb.blue_pos;
    return color;
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

void colors_init(void);

#endif