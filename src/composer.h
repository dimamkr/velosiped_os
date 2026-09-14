#ifndef COMPOSER
#define COMPOSER

#include "types.h"

#define LAYER_PRESENT 1
#define LAYER_VISIBLE 2
#define LAYER_OPAQUE 4 // непрозрачный
#define LAYER_BUSY 8   // прямо сейчас изменяется

// TODO двойная буферизация слоя для того, чтобы не было графических артефактов
// TODO обработка dirty - не флашить если ничего не изменилось
// TODO допустимость отрицательных координат
// TODO клиппинг (если слой выходит за экран). переход на int вместо uint
// TODO блокировка слоев пока рисуем их LAYER_BUSY
// TODO разделение пометки на удаление и реального удаления
// TODO alpha blending

typedef struct
{
    uint32_t x_left;
    uint32_t y_up;
    uint32_t x_len;
    uint32_t y_len;

    uint32_t *buff;

    uint32_t z_index; // чем меньше значение тем раньше рисуется
    uint32_t flags;
    uint8_t opacity; // TODO
} layer_t;

layer_t *layer_create(void);
void layer_init(layer_t *this, uint32_t x_left, uint32_t y_up, uint32_t x_len, uint32_t y_len, uint32_t z_index);
void layer_destroy(layer_t *this);
void layer_clean(layer_t *this);
void layer_put_pixel(layer_t *this, uint32_t x_local, uint32_t y_local, uint32_t color);

layer_t *composer_create_layer(uint32_t x_left, uint32_t y_up, uint32_t x_len, uint32_t y_len, uint32_t z_index);
void composer_add_layer(layer_t *this);
void composer_erase_layer(layer_t *this);

void composer_init(void);
void composer_flush_prepare(void);
void composer_flush(void);

#endif