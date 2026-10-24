#include "syscall.h"
#include "input.h"
#include "windows.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* нейрозмейка */

#define GRID_W 20
#define GRID_H 15
#define CELL 16

#define WIN_W (GRID_W * CELL)
#define WIN_H (GRID_H * CELL)

#define MAX_SNAKE 300
#define TICK_MS 120

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

static uint32_t *framebuf;

/* --- рисование --- */

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

/* --- еда --- */

static void place_food(point_t *food, const point_t *snake, int len)
{
    for (int tries = 0; tries < 2000; ++tries)
    {
        int x = rand() % GRID_W;
        int y = rand() % GRID_H;
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
    food->x = -1; /* всё занято — победа */
    food->y = -1;
}

/* --- игра --- */

int main(void)
{
    int win = sys_win_create(WIN_W, WIN_H, 120, 90, 500);
    if (win < 0)
        return 1;

    framebuf = malloc(WIN_W * WIN_H * sizeof(uint32_t));
    if (!framebuf)
    {
        sys_win_destroy(win);
        return 1;
    }

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
        draw_board();
        if (food.x >= 0)
            draw_cell(food.x, food.y, COLOR_FOOD);
        for (int i = 0; i < len; ++i)
            draw_cell(snake[i].x, snake[i].y, i == 0 ? COLOR_HEAD : COLOR_SNAKE);
        sys_win_commit(win, framebuf);

        sys_sleep(TICK_MS);

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
                free(framebuf);
                sys_win_destroy(win);
                return 0;
            }
        }

        dx = ndx;
        dy = ndy;

        point_t head = {snake[0].x + dx, snake[0].y + dy};

        if (head.x < 0 || head.x >= GRID_W ||
            head.y < 0 || head.y >= GRID_H)
            break;

        int eating = (head.x == food.x && head.y == food.y);

        int check = eating ? len : len - 1;
        for (int i = 0; i < check; ++i)
            if (snake[i].x == head.x && snake[i].y == head.y)
                goto game_over;

        if (eating)
        {
            if (len < MAX_SNAKE)
                len++;
            place_food(&food, snake, len);
            printf("%d\n", len);
        }
        for (int i = len - 1; i > 0; --i)
            snake[i] = snake[i - 1];
        snake[0] = head;

        if (food.x < 0)
            break;
    }

game_over:
    for (int i = 0; i < WIN_W * WIN_H; ++i)
        framebuf[i] = COLOR_DEAD;
    sys_win_commit(win, framebuf);
    sys_sleep(2000);

    free(framebuf);
    sys_win_destroy(win);
    return 0;
}