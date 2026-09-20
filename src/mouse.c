// mouse.c
#include "mouse.h"
#include "system.h"
#include "isr.h"
#include "framebuffer.h"
#include "ring.h"

#pragma GCC optimize("no-optimize-sibling-calls")

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_CMD 0x64

#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_INPUT_FULL 0x02
#define PS2_STATUS_MOUSE_BIT 0x20

#define MOUSE_RING_SIZE 4096

#define MOUSE_DELAY 100000

static ring_t *mouse_ring;

// ожидание пока порт занят
static void ps2_wait_write(void)
{
    for (uint32_t i = 0; i < MOUSE_DELAY; i++)
        if (!(inb(PS2_STATUS) & PS2_STATUS_INPUT_FULL))
            return;
}

// ожидание пока не положат данные в порт
static void ps2_wait_read(void)
{
    for (uint32_t i = 0; i < MOUSE_DELAY; i++)
        if (inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL)
            return;
}

// отправка данных ps2 контроллеру
static void ps2_cmd(uint8_t cmd)
{
    ps2_wait_write();
    outb(PS2_CMD, cmd);
}

// отправка данных для мыши через ps2 контроллер
static void ps2_data(uint8_t data)
{
    ps2_wait_write();
    outb(PS2_DATA, data);
}

// отправить команду мыши
static bool_t mouse_send(uint8_t cmd)
{
    ps2_cmd(0xD4); // говорим что дадим команду
    ps2_data(cmd);
    ps2_wait_read();

    uint8_t ack = inb(PS2_DATA);
    return ack == 0xFA; // мышь получила команду
}

// получение настроек контроллера ps2
static uint8_t ps2_read_config(void)
{
    ps2_cmd(0x20); // говорим что хотим получить конфигурацию контроллера
    ps2_wait_read();
    return inb(PS2_DATA);
}

static void ps2_write_config(uint8_t cfg)
{
    ps2_cmd(0x60);
    ps2_data(cfg);
}

__attribute__((optimize("O0"))) void mouse_init(void)
{
    interrupt_disable();

    // 1. включить второй канал (для мыши)
    ps2_cmd(0xA8);

    // 2. установить нужные настройки ps2
    uint8_t cfg = ps2_read_config();
    cfg |= 0x02;  // бит 1: mouse interrupt enable
    cfg &= ~0x20; // бит 5: не игнорировать clock мыши
    ps2_write_config(cfg);

    // 3. режим работы по умолчанию
    mouse_send(0xF6); // 100 гц 100 dpi, сброс внутренних буферов мыши, перевод в режим ожидания

    // 4. выход из режима ожидания
    mouse_send(0xF4);

    // 5. "холостое" считывание всех данных которые могли накопиться в контроллере для очистки его буферов
    // также игнорирование ответа на пред команду
    for (int i = 0; i < MOUSE_DELAY; ++i)
    {
    }
    {
        while (inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL)
            inb(PS2_DATA);
    }

    // 6. зарегистрировать IRQ12
    interrupt_register(IRQ12, mouse_top_callback, mouse_bottom_callback);

    mouse_ring = ring_create(sizeof(byte_t), MOUSE_RING_SIZE);

    // TODO ВЫНЕСТИ ОТДЕЛЬНО
    mouse_cursor_init();

    interrupt_enable();
}

// мышь генерирует прерывание на каждый отправленнный байт
void mouse_top_callback(isr_data_t data)
{
    uint8_t status = inb(PS2_STATUS);

    // проверка: выходной буфер полон и байт от мыши (бит 5)
    if (!(status & PS2_STATUS_OUTPUT_FULL))
        return;
    if (!(status & PS2_STATUS_MOUSE_BIT))
        return;

    uint8_t byte = inb(PS2_DATA);

    ring_push_back(mouse_ring, &byte);
}

static uint8_t packet[3];
static uint8_t packet_idx = 0;
static int32_t mouse_x = 512;
static int32_t mouse_y = 384;
static bool_t buttons_prev[3] = {0};

void mouse_bottom_callback(isr_data_t data)
{
    uint8_t byte;

    while (!ring_empty(mouse_ring))
    {
        ring_pop_front(mouse_ring, &byte);

        // проверка: первый байт пакета всегда имеет бит 3 == 1
        if (packet_idx == 0 && !(byte & 0x08))
            continue;

        packet[packet_idx++] = byte;
        if (packet_idx < 3)
            continue;

        // Считали стандартный пакет из 3 байт. Расширение до 4 байт (колесико) в данном режиме контроллера отключено
        packet_idx = 0;

        uint8_t flags = packet[0];
        int32_t dx = (int32_t)packet[1];
        int32_t dy = (int32_t)packet[2];

        // биты знака
        if (flags & 0x10)
            dx -= 256;
        if (flags & 0x20)
            dy -= 256;

        // движение
        mouse_x += dx;
        mouse_y -= dy; // y инвертирована

        // границы экрана
        if (mouse_x < 0)
            mouse_x = 0;
        if (mouse_y < 0)
            mouse_y = 0;
        if (mouse_x >= (int32_t)framebuffer.width)
            mouse_x = framebuffer.width - 1;
        if (mouse_y >= (int32_t)framebuffer.height)
            mouse_y = framebuffer.height - 1;

        // Кнопки
        bool_t left = flags & 0x01;
        bool_t right = flags & 0x02;
        bool_t middle = flags & 0x04;

        // Обновить курсор в композиторе
        mouse_cursor_move(mouse_x, mouse_y);
    }
}

#define CURSOR_W 12
#define CURSOR_H 19

#include "composer.h"
#include "colors.h"
static layer_t *cursor_layer;
static int32_t cursor_last_x = -1, cursor_last_y = -1;

// Bitmap курсора: 0 = прозрачный, 1 = обводка, 2 = осн цвет
static const uint8_t cursor_bitmap[CURSOR_H][CURSOR_W] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0},
    {1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 0},
    {1, 2, 2, 1, 2, 2, 1, 0, 0, 0, 0, 0},
    {1, 2, 1, 0, 1, 2, 2, 1, 0, 0, 0, 0},
    {1, 1, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0},
};

#include "konsole.h"
void mouse_cursor_init(void)
{
    cursor_layer = composer_create_layer(512, 384, CURSOR_W, CURSOR_H,
                                         1000, LAYER_VISIBLE);

    LAYER_EDIT_FUNCTION(cursor_layer);
    for (int y = 0; y < CURSOR_H; y++)
    {
        for (int x = 0; x < CURSOR_W; x++)
        {
            uint32_t c;
            switch (cursor_bitmap[y][x])
            {
            case 0:
                c = RGBA(0, 0, 0, 0);
                break; // прозрачный
            case 1:
                c = RGB_TO_RGBA(color_white, 255);
                break; // обводка
            case 2:
                c = RGB_TO_RGBA(color_black, 255);
                break; // цвет
            }
            layer_put_pixel(cursor_layer, x, y, c);
        }
    }
}

void mouse_cursor_move(int32_t x, int32_t y)
{
    cursor_layer->x_left = x;
    cursor_layer->y_up = y;

    layer_move(cursor_layer, x, y);
}