#ifndef COMPOSER
#define COMPOSER

#include "types.h"
#include "system.h"

#define _LAYER_PRESENT 1
#define LAYER_VISIBLE 2
#define LAYER_OPAQUE 4  // непрозрачный
#define _LAYER_BUSY 8   // прямо сейчас изменяется (для отсутствия артефактов)
#define _LAYER_DIRTY 16 // требуется синхронизация buff2 с buff

// TODO двойная буферизация слоя для того, чтобы не было графических артефактов
// TODO обработка dirty - не флашить если ничего не изменилось
// TODO допустимость отрицательных координат
// TODO клиппинг (если слой выходит за экран). переход на int вместо uint
// TODO блокировка слоев пока рисуем их LAYER_BUSY
// TODO разделение пометки на удаление и реального удаления
// TODO alpha blending

typedef struct
{
    int32_t x_left;
    int32_t y_up;
    int32_t x_len;
    int32_t y_len;

    uint32_t *buff;  // всегда будет рисоваться именно этот
    uint32_t *buff2; // для всех промежуточных изменений

    int32_t busy_lock; // (аналогичен счетчику lock в многозадачности)

    int32_t z_index; // чем меньше значение тем раньше рисуется
    uint32_t flags;
    uint8_t global_opacity; // TODO
} layer_t;

layer_t *layer_create(void);
void layer_init(layer_t *this, int32_t x_left, int32_t y_up, int32_t x_len, int32_t y_len, int32_t z_index, uint32_t flags);
void layer_destroy(layer_t *this);
void layer_clean(layer_t *this);
void layer_begin_edit(layer_t *this);
void layer_end_edit(layer_t *this);

static inline void layer_put_pixel(layer_t *this, int32_t x_local, int32_t y_local, uint32_t color_rgba)
{
    ASSERT(this->busy_lock > 0); // слой обязан быть в состоянии изменения
    if (x_local >= this->x_len || y_local >= this->y_len || y_local < 0 || x_local < 0)
        return;

    this->buff2[y_local * this->x_len + x_local] = color_rgba;
}

void layer_move(layer_t *this, int32_t new_x, int32_t new_y);

layer_t *composer_create_layer(int32_t x_left, int32_t y_up, int32_t x_len, int32_t y_len, int32_t z_index, uint32_t flags);
void composer_add_layer(layer_t *this);
void composer_erase_layer(layer_t *this);

void composer_init(void);
void composer_flush_prepare(void);
void composer_flush(void);

static inline void __layer_edit_end_trick(layer_t **layer)
{
    layer_end_edit(*layer);
}

#define LAYER_EDIT_FUNCTION(layer_ptr)                                                                                    \
    __attribute__((unused)) layer_t *__layer_edit_end_guard __attribute__((cleanup(__layer_edit_end_trick))) = layer_ptr; \
    layer_begin_edit(layer_ptr);

#endif