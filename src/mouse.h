#include "isr.h"

void mouse_init(void);
void mouse_cursor_init(void);

void mouse_top_callback(isr_data_t data);
void mouse_bottom_callback(isr_data_t data);

void mouse_cursor_move(int32_t x, int32_t y);