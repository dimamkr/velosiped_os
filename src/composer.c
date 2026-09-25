#include "composer.h"
#include "heap.h"
#include "dynamic_array.h"
#include "colors.h"
#include "framebuffer.h"
#include "renderer.h"
#include "task.h"

dynamic_array_t *layers;    // хранит layer_t*
static bool_t layers_dirty; // хоть какой-то слой в массиве изменился

#define BUFF_IDX(layer, y, x) (y * layer->x_len + x)

bool_t layer_less(void *_a, void *_b)
{
    layer_t *a = *(layer_t **)_a;
    layer_t *b = *(layer_t **)_b;

    if (a->flags & _LAYER_PRESENT)
    {
        return b->flags & _LAYER_PRESENT ? a->z_index < b->z_index : true;
    }

    return b->flags & _LAYER_PRESENT ? false : a->z_index < b->z_index;
}

layer_t *layer_create(void)
{
    layer_t *r = malloc(sizeof(layer_t));
    return r;
}

void layer_begin_edit(layer_t *this)
{
    TASK_LOCKED_FUNCTION;

    if (!this->busy_lock)
    {
        // будет рисоваться версия с прошлых изменений (текущий buff2)
        memswap(&this->buff, &this->buff2, sizeof(uint32_t *));

        this->flags |= _LAYER_BUSY;
        this->flags |= _LAYER_DIRTY;
    }

    this->busy_lock++;
}

void layer_end_edit(layer_t *this)
{
    TASK_LOCKED_FUNCTION;

    --this->busy_lock;
    if (!this->busy_lock)
    {
        // будет рисоваться новая версия (текущий buff2)
        memswap(&this->buff, &this->buff2, sizeof(uint32_t *));

        this->flags &= ~_LAYER_BUSY;

        if (this->flags & _LAYER_PRESENT)
            layers_dirty = true;
    }
}

// сам инициализирует буффер
void layer_init(layer_t *this, int32_t x_left, int32_t y_up, int32_t x_len, int32_t y_len, int32_t z_index, uint32_t flags)
{
    this->buff = malloc(x_len * y_len * SIZEOF_PIXEL);
    this->buff2 = malloc(x_len * y_len * SIZEOF_PIXEL);
    this->busy_lock = 0;

    this->x_left = x_left;
    this->y_up = y_up;
    this->x_len = x_len;
    this->y_len = y_len;
    this->z_index = z_index;

    this->flags = flags;

    layer_clean(this);
}

void layer_clean(layer_t *this)
{
    for (uint32_t id = 0; id < this->x_len * this->y_len; ++id)
    {
        this->buff[id] = color_black;
        this->buff2[id] = color_black;
    }
}

void layer_destroy(layer_t *this)
{
    free(this->buff);
    free(this->buff2);
    free(this);
}

// для перемещения слоя
// использовать строго эту функцию для изменения его координат!!!!
void layer_move(layer_t *this, int32_t new_x, int32_t new_y)
{
    TASK_LOCKED_FUNCTION;
    this->x_left = new_x;
    this->y_up = new_y;
    if (this->flags & _LAYER_PRESENT)
        layers_dirty = true;
}

// сразу создает слой который кладется в массив рисуемых
// важно установить флаг LAYER_VISIBLE вручную
layer_t *composer_create_layer(int32_t x_left, int32_t y_up, int32_t x_len, int32_t y_len, int32_t z_index, uint32_t flags)
{
    layer_t *r = layer_create();
    layer_init(r, x_left, y_up, x_len, y_len, z_index, flags);
    composer_add_layer(r);

    return r;
}

void composer_add_layer(layer_t *this)
{
    TASK_LOCKED_FUNCTION;

    this->flags |= _LAYER_PRESENT;
    dynamic_array_push_back(layers, &this);
}

void composer_erase_layer(layer_t *this)
{
    TASK_LOCKED_FUNCTION;

    uint32_t id = dynamic_array_find_first_eq(layers, &this);
    ASSERT(id < layers->elements_count); // слой найден

    (*(layer_t **)dynamic_array_get_by_index(layers, id))->flags &= ~_LAYER_PRESENT;
}

void composer_compose_layer(layer_t *this)
{
    if ((this->flags & _LAYER_PRESENT) && (this->flags & LAYER_VISIBLE))
    {
        if ((this->flags & _LAYER_DIRTY) && !(this->flags & _LAYER_BUSY))
        {
            memcpy_xl(this->buff2, this->buff, this->x_len * this->y_len * SIZEOF_PIXEL);
            this->flags &= ~_LAYER_DIRTY;
        }

        if (this->flags & LAYER_OPAQUE)
        {
            for (uint32_t y = 0; y < this->y_len; ++y)
            {
                framebuffer_put_xline(this->buff + BUFF_IDX(this, y, 0), this->x_left, this->y_up + y, this->x_len);
            }
        }
        else
        {
            for (uint32_t y = 0; y < this->y_len; ++y)
            {
                for (uint32_t x = 0; x < this->x_len; ++x)
                {
                    framebuffer_put_pixel(this->x_left + x, this->y_up + y, this->buff[BUFF_IDX(this, y, x)]);
                }
            }
        }
    }
}

void composer_flush_prepare(void)
{
    TASK_LOCKED_FUNCTION;

    dynamic_array_quicksort(layers, 0, layers->elements_count - 1, layer_less);
    while (layers->elements_count > 0 && !((*(layer_t **)dynamic_array_get_top(layers))->flags & _LAYER_PRESENT))
    {
        layer_destroy(*(layer_t **)dynamic_array_get_top(layers));
        dynamic_array_pop_back(layers);
    }
}

void composer_flush(void)
{
    TASK_LOCKED_FUNCTION;

    for (uint32_t id = 0; id < layers->elements_count; ++id)
    {
        composer_compose_layer(*(layer_t **)dynamic_array_get_by_index(layers, id));
    }
    layers_dirty = false;
}

void composer_task(void *_)
{
    (void)_;

    while (true)
    {
        task_sleep(17); //~60 фпс

        if (!layers_dirty)
            continue;

        composer_flush_prepare();
        composer_flush();
        framebuffer_flush();
    }
}

void composer_init(void)
{
    layers = dynamic_array_create(sizeof(layer_t *));
    layers_dirty = true;
    task_create_kthread(composer_task, NULL, STACK_SIZE_LARGE);
}
