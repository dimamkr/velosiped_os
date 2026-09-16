#include "colors.h"

uint32_t color_light_gray;
uint32_t color_black;
uint32_t color_light_green;
uint32_t color_yellow;
uint32_t color_red;
uint32_t color_light_red;
uint32_t color_white;
uint32_t color_light_blue;
uint32_t color_green;
uint32_t color_blue;

void colors_init(void)
{
    color_light_gray = RGB(170, 170, 170);
    color_black = RGB(0, 0, 0);
    color_light_green = RGB(0, 255, 0);
    color_yellow = RGB(255, 255, 0);
    color_red = RGB(170, 0, 0);
    color_light_red = RGB(255, 85, 85);
    color_white = RGB(255, 255, 255);
    color_light_blue = RGB(85, 85, 255);
    color_green = RGB(0, 255, 0);
    color_blue = RGB(0, 0, 255);
}
