#include "isr.h"

typedef struct
{
    int8_t dx;       // смещение по X, положительное = вправо
    int8_t dy;       // смещение по Y, положительное = вниз
    uint8_t buttons; // битовая маска нажатых кнопок
    int8_t wheel;    // прокрутка: -1, 0, +1; 0
} mouse_event_t;

// маски кнопок
#define MOUSE_BUTTON_LEFT 0x01
#define MOUSE_BUTTON_RIGHT 0x02
#define MOUSE_BUTTON_MIDDLE 0x04
#define MOUSE_BUTTON_4 0x08 // боковая
#define MOUSE_BUTTON_5 0x10 // боковая

void mouse_init(void);
void mouse_cursor_init(void);

void mouse_top_callback(isr_data_t data);
void mouse_bottom_callback(isr_data_t data);

void mouse_cursor_move(int32_t x, int32_t y);