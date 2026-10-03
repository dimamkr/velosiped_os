#include "unistd.h"
#include "input.h"
#include <stdint.h>

// нейрозмейка

#define GRID_W 20
#define GRID_H 15
#define CELL 16

#define WIN_W (GRID_W * CELL)
#define WIN_H (GRID_H * CELL)

#define MAX_SNAKE 300
#define TICK_MS 120

#define COLOR_BG 0xFF101020
#define COLOR_GRID_A 0xFF181828
#define COLOR_GRID_B 0xFF202030
#define COLOR_SNAKE 0xFF30D030
#define COLOR_HEAD 0xFF90FF90
#define COLOR_FOOD 0xFFE03030
#define COLOR_DEAD 0xFF800000

typedef struct
{
    int x, y;
} point_t;

static uint32_t framebuf[WIN_W * WIN_H];

// --- рисование ---

static void fill_rect(int x, int y, int w, int h, uint32_t color)
{
    if (x < 0)
    {
        w += x;
        x = 0;
    }
    if (y < 0)
    {
        h += y;
        y = 0;
    }
    if (x + w > WIN_W)
        w = WIN_W - x;
    if (y + h > WIN_H)
        h = WIN_H - y;
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            framebuf[(y + j) * WIN_W + (x + i)] = color;
}

static void draw_board(void)
{
    for (int y = 0; y < GRID_H; ++y)
        for (int x = 0; x < GRID_W; ++x)
            fill_rect(x * CELL, y * CELL, CELL, CELL,
                      ((x + y) & 1) ? COLOR_GRID_A : COLOR_GRID_B);
}

static void draw_cell(int gx, int gy, uint32_t color)
{
    fill_rect(gx * CELL + 1, gy * CELL + 1, CELL - 2, CELL - 2, color);
}

// --- рандом ---

static uint32_t rng_state = 0xC0FFEE;

static uint32_t rng(void)
{
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state >> 16;
}

static void place_food(point_t *food, point_t *snake, int len)
{
    for (int tries = 0; tries < 2000; ++tries)
    {
        int x = rng() % GRID_W;
        int y = rng() % GRID_H;
        int ok = 1;
        for (int i = 0; i < len; ++i)
            if (snake[i].x == x && snake[i].y == y)
            {
                ok = 0;
                break;
            }
        if (ok)
        {
            food->x = x;
            food->y = y;
            return;
        }
    }
    // всё занято — победа
    food->x = -1;
    food->y = -1;
}

// --- игра ---

void _start(void)
{
    int win = sys_win_create(WIN_W, WIN_H, 120, 90, 500);
    if (win < 0)
        sys_exit(1);

    point_t snake[MAX_SNAKE];
    int len = 3;
    snake[0] = (point_t){GRID_W / 2, GRID_H / 2};
    snake[1] = (point_t){GRID_W / 2 - 1, GRID_H / 2};
    snake[2] = (point_t){GRID_W / 2 - 2, GRID_H / 2};

    int dx = 1, dy = 0;
    int ndx = 1, ndy = 0;

    point_t food;
    place_food(&food, snake, len);

    keyboard_event_t ev;

    for (;;)
    {
        // --- отрисовка ---
        draw_board();
        if (food.x >= 0)
            draw_cell(food.x, food.y, COLOR_FOOD);
        for (int i = 0; i < len; ++i)
            draw_cell(snake[i].x, snake[i].y, i == 0 ? COLOR_HEAD : COLOR_SNAKE);
        sys_win_commit(win, framebuf);

        // --- спим тик ---
        sys_sleep(TICK_MS);

        // --- читаем все накопившиеся нажатия ---
        while (sys_poll(0))
        {
            if (sys_read(0, &ev, sizeof(ev)) != sizeof(ev))
                break;
            if (!ev.pressed)
                continue;

            switch (ev.keycode)
            {
            case KEY_UP:
                if (dy != 1)
                {
                    ndx = 0;
                    ndy = -1;
                }
                break;
            case KEY_DOWN:
                if (dy != -1)
                {
                    ndx = 0;
                    ndy = 1;
                }
                break;
            case KEY_LEFT:
                if (dx != 1)
                {
                    ndx = -1;
                    ndy = 0;
                }
                break;
            case KEY_RIGHT:
                if (dx != -1)
                {
                    ndx = 1;
                    ndy = 0;
                }
                break;
            case KEY_ESC:
                sys_win_destroy(win);
                sys_exit(0);
            }
        }

        dx = ndx;
        dy = ndy;

        // --- движение ---
        point_t head = {snake[0].x + dx, snake[0].y + dy};

        // стена
        if (head.x < 0 || head.x >= GRID_W ||
            head.y < 0 || head.y >= GRID_H)
            break;

        int eating = (head.x == food.x && head.y == food.y);

        // хвост, если не растём, не считается — он уйдёт с этой клетки
        int check = eating ? len : len - 1;
        for (int i = 0; i < check; ++i)
            if (snake[i].x == head.x && snake[i].y == head.y)
            {
                sys_win_destroy(win);
                sys_exit(0);
            }

        // сдвиг тела в хвост
        if (eating)
        {
            if (len < MAX_SNAKE)
                len++;
            place_food(&food, snake, len);
        }
        for (int i = len - 1; i > 0; --i)
            snake[i] = snake[i - 1];
        snake[0] = head;

        // победа — если вся сетка занята
        if (food.x < 0)
            break;
    }

    // --- game over ---
    for (int i = 0; i < WIN_W * WIN_H; ++i)
        framebuf[i] = COLOR_DEAD;
    sys_win_commit(win, framebuf);
    sys_sleep(2000);

    sys_win_destroy(win);
    sys_exit(0);
}