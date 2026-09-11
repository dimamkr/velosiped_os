#include "colors.h"

uint32_t color_light_gray;
uint32_t color_black;
uint32_t color_light_green;
uint32_t color_yellow;
uint32_t color_red;
uint32_t color_light_red;
uint32_t color_white;
uint32_t color_light_blue;

void colors_init(void)
{
    color_light_gray = color_to_pixel(170, 170, 170);
    color_black = color_to_pixel(0, 0, 0);
    color_light_green = color_to_pixel(0, 255, 0);
    color_yellow = color_to_pixel(255, 255, 0);
    color_red = color_to_pixel(170, 0, 0);
    color_light_red = color_to_pixel(255, 85, 85);
    color_white = color_to_pixel(255, 255, 255);
    color_light_blue = color_to_pixel(85, 85, 255);
}
