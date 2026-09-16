#ifndef FONT_H
#define FONT_H

#include "types.h"

#define FONT_WIDTH 8   // биты
#define FONT_HEIGHT 16 // байты

#define FONT_GLYPH_SIZE 16                                                               // число байт на символ
#define FONT_GET_BIT(glyph_ptr, y, x) (((*(glyph_ptr + y)) >> (FONT_WIDTH - 1 - x)) & 1) // y ряд x колонка

extern uint8_t font8x16_vga[];

#endif