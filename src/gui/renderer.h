#ifndef RENDERER
#define RENDERER

#include "types.h"
#include "composer.h"

void renderer_draw_char(layer_t *layer, uint8_t *font8x16, char symbol, uint32_t x_left, uint32_t y_up, uint32_t fg_color, uint32_t bg_color);
void renderer_draw_rect(layer_t *layer, uint32_t x_left, uint32_t y_up, uint32_t x_len, uint32_t y_len, uint32_t color);

#endif