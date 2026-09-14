#include "composer.h"
#include "heap.h"
#include "dynamic_array.h"
#include "colors.h"
#include "framebuffer.h"
#include "renderer.h"
#include "task.h"

dynamic_array_t *layers; // хранит layer_t*

#define BUFF_IDX(layer, y, x) (y * layer->x_len + x)

bool_t layer_less(void *_a, void *_b)
{
    layer_t *a = *(layer_t **)_a;
    layer_t *b = *(layer_t **)_b;

    if (a->flags & LAYER_PRESENT)
    {
        return b->flags & LAYER_PRESENT ? a->z_index < b->z_index : true;
    }

    return b->flags & LAYER_PRESENT ? false : a->z_index < b->z_index;
}

layer_t *layer_create(void)
{
    layer_t *r = malloc(sizeof(layer_t));
    return r;
}

// сам инициализирует буффер
void layer_init(layer_t *this, uint32_t x_left, uint32_t y_up, uint32_t x_len, uint32_t y_len, uint32_t z_index)
{
    this->buff = malloc(x_len * y_len * SIZEOF_PIXEL);

    this->x_left = x_left;
    this->y_up = y_up;
    this->x_len = x_len;
    this->y_len = y_len;
    this->z_index = z_index;

    this->flags = LAYER_PRESENT | LAYER_OPAQUE;

    layer_clean(this);
}

void layer_clean(layer_t *this)
{
    for (uint32_t id = 0; id < this->x_len * this->y_len; ++id)
    {
        this->buff[id] = color_black;
    }
}

void layer_destroy(layer_t *this)
{
    free(this->buff);
    free(this);
}

void layer_put_pixel(layer_t *this, uint32_t x_local, uint32_t y_local, uint32_t color_rgba)
{
    if (x_local >= this->x_len || y_local >= this->y_len)
        return;

    this->buff[BUFF_IDX(this, y_local, x_local)] = color_rgba;
}

// сразу делает все что нужно со слоем
layer_t *composer_create_layer(uint32_t x_left, uint32_t y_up, uint32_t x_len, uint32_t y_len, uint32_t z_index)
{
    layer_t *r = layer_create();
    layer_init(r, x_left, y_up, x_len, y_len, z_index);
    composer_add_layer(r);

    return r;
}

void composer_add_layer(layer_t *this)
{
    dynamic_array_push_back(layers, &this);
}

void composer_erase_layer(layer_t *this)
{
    uint32_t id = dynamic_array_find_first_eq(layers, &this);
    ASSERT(id < layers->elements_count); // слой найден

    (*(layer_t **)dynamic_array_get_by_index(layers, id))->flags &= ~LAYER_PRESENT;
}

void composer_compose_layer(layer_t *this)
{
    if (!(this->flags & LAYER_PRESENT))
        return;

    for (uint32_t y = 0; y < this->y_len; ++y)
    {
        for (uint32_t x = 0; x < this->x_len; ++x)
        {
            framebuffer_put_pixel(this->x_left + x, this->y_up + y, this->buff[BUFF_IDX(this, y, x)]);
        }
    }
}

void composer_flush_prepare(void)
{
    dynamic_array_quicksort(layers, 0, layers->elements_count - 1, layer_less);
    while (layers->elements_count > 0 && !((*(layer_t **)dynamic_array_get_top(layers))->flags & LAYER_PRESENT))
    {
        layer_destroy(*(layer_t **)dynamic_array_get_top(layers));
        dynamic_array_pop_back(layers);
    }
}

void composer_flush(void)
{
    framebuffer_is_busy = true;
    for (uint32_t id = 0; id < layers->elements_count; ++id)
    {
        composer_compose_layer(*(layer_t **)dynamic_array_get_by_index(layers, id));
    }
    framebuffer_is_busy = false;
}

void composer_task(void *_)
{
    (void)_;

    while (true)
    {
        task_sleep(17); //~60 фпс

        composer_flush_prepare();
        composer_flush();
        framebuffer_flush();
    }
}

void composer_init(void)
{
    layers = dynamic_array_create(sizeof(layer_t *));
    task_create(composer_task, NULL, STACK_SIZE_LARGE);
}
