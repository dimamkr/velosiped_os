#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"
#include "isr.h"
#include "system.h"
#include "pic.h"
#include "task_event.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// Модификаторы (битовые маски)
#define MOD_SHIFT 0x01
#define MOD_CTRL 0x02
#define MOD_ALT 0x04
#define MOD_CAPSLOCK 0x08
#define MOD_NUMLOCK 0x10

// Специальные коды клавиш (для keycode, >= 0x80)
#define KEY_ESC 0x80
#define KEY_BACKSPACE 0x81
#define KEY_TAB 0x82
#define KEY_ENTER 0x83
#define KEY_SPACE 0x84
#define KEY_UP 0x85
#define KEY_DOWN 0x86
#define KEY_LEFT 0x87
#define KEY_RIGHT 0x88
#define KEY_F1 0x89
#define KEY_F2 0x8A
#define KEY_F3 0x8B
#define KEY_F4 0x8C
#define KEY_F5 0x8D
#define KEY_F6 0x8E
#define KEY_F7 0x8F
#define KEY_F8 0x90
#define KEY_F9 0x91
#define KEY_F10 0x92
#define KEY_F11 0x93
#define KEY_F12 0x94
#define KEY_INSERT 0x95
#define KEY_DELETE 0x96
#define KEY_HOME 0x97
#define KEY_END 0x98
#define KEY_PAGEUP 0x99
#define KEY_PAGEDOWN 0x9A

// Структура события клавиатуры
typedef struct
{
    uint8_t scancode;  // сырой скан-код
    uint8_t keycode;   // ASCII или специальный код (>=0x80)
    uint8_t modifiers; // битовая маска MOD_*
    bool pressed;      // true – нажатие, false – отпускание
} keyboard_event_t;

// Инициализация драйвера
void keyboard_init(void);

// Извлечь событие из очереди (для терминала)
bool keyboard_dequeue_event(keyboard_event_t *ev);

// Callback'и для регистрации в isr.c
void keyboard_top_callback(isr_data_t data);
void keyboard_bottom_callback(isr_data_t data);

// Глобальное событие для пробуждения терминала
extern task_event_t *keyboard_event;

#endif