#include "keyboard.h"
#include "terminal.h"
#include "system.h"
#include "konsole.h"
#include "task_event.h"
#include "heap.h"

#pragma GCC optimize("no-optimize-sibling-calls")

// ------------------------------------------------------------
// Буферы и очереди
// ------------------------------------------------------------

#define SCANCODE_BUFFER_SIZE 32
static uint8_t scancode_buffer[SCANCODE_BUFFER_SIZE];
static volatile int scancode_buffer_head = 0;
static volatile int scancode_buffer_tail = 0;

#define EVENT_QUEUE_SIZE 64
static keyboard_event_t event_queue[EVENT_QUEUE_SIZE];
static volatile int event_queue_head = 0;
static volatile int event_queue_tail = 0;

static uint8_t current_modifiers = 0;

task_event_t *keyboard_event = NULL;

// ------------------------------------------------------------
// Таблицы перекодировки (скан-код -> ASCII)
// ------------------------------------------------------------

// Без модификаторов
static const char scancode_ascii[] = {
    '\0', 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
    0, 0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    0, 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0, '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// с shiftом
static const char scancode_shifted[] = {
    '\0', 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
    0, 0, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    0, 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    0, '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// ------------------------------------------------------------
// Вспомогательные функции
// ------------------------------------------------------------

static bool scancode_buffer_push(uint8_t sc)
{
    int next = (scancode_buffer_tail + 1) % SCANCODE_BUFFER_SIZE;
    if (next == scancode_buffer_head)
        return false;
    scancode_buffer[scancode_buffer_tail] = sc;
    scancode_buffer_tail = next;
    return true;
}

static bool scancode_buffer_pop(uint8_t *sc)
{
    if (scancode_buffer_head == scancode_buffer_tail)
        return false;
    *sc = scancode_buffer[scancode_buffer_head];
    scancode_buffer_head = (scancode_buffer_head + 1) % SCANCODE_BUFFER_SIZE;
    return true;
}

static bool event_queue_push(keyboard_event_t *ev)
{
    int next = (event_queue_tail + 1) % EVENT_QUEUE_SIZE;
    if (next == event_queue_head)
        return false;
    event_queue[event_queue_tail] = *ev;
    event_queue_tail = next;
    return true;
}

bool keyboard_dequeue_event(keyboard_event_t *ev)
{
    if (event_queue_head == event_queue_tail)
        return false;
    *ev = event_queue[event_queue_head];
    event_queue_head = (event_queue_head + 1) % EVENT_QUEUE_SIZE;
    return true;
}

// Обновление модификаторов при нажатии/отпускании
static void update_modifiers(uint8_t scancode, bool pressed)
{
    // вырезаем бит отпускания
    scancode &= (uint32_t)~0x80;

    switch (scancode)
    {
    case 0x2A: // Left Shift
        if (pressed)
            current_modifiers |= MOD_SHIFT;
        else
            current_modifiers &= ~MOD_SHIFT;
        break;
    case 0x36: // Right Shift
        if (pressed)
            current_modifiers |= MOD_SHIFT;
        else
            current_modifiers &= ~MOD_SHIFT;
        break;
    case 0x1D: // Ctrl
        if (pressed)
            current_modifiers |= MOD_CTRL;
        else
            current_modifiers &= ~MOD_CTRL;
        break;
    case 0x38: // Alt
        if (pressed)
            current_modifiers |= MOD_ALT;
        else
            current_modifiers &= ~MOD_ALT;
        break;
    case 0x3A: // CapsLock
        if (pressed)
            current_modifiers ^= MOD_CAPSLOCK;
        break;
        // NumLock, ScrollLock можно добавить при необходимости
    }
}

// Преобразование скан-кода в keycode (ASCII или спец)
static uint8_t scancode_to_keycode(uint8_t scancode, uint8_t modifiers)
{
    // Отпускание игнорируем
    if (scancode & 0x80)
        return 0;

    bool shift = (modifiers & MOD_SHIFT) != 0;
    bool capslock = (modifiers & MOD_CAPSLOCK) != 0;
    bool ctrl = (modifiers & MOD_CTRL) != 0;

    // Специальные клавиши
    switch (scancode)
    {
    case 0x01:
        return KEY_ESC;
    case 0x0E:
        return KEY_BACKSPACE;
    case 0x0F:
        return KEY_TAB;
    case 0x1C:
        return KEY_ENTER;
    case 0x39:
        return KEY_SPACE;
    case 0x47:
        return KEY_HOME;
    case 0x48:
        return KEY_UP;
    case 0x49:
        return KEY_PAGEUP;
    case 0x4B:
        return KEY_LEFT;
    case 0x4D:
        return KEY_RIGHT;
    case 0x4F:
        return KEY_END;
    case 0x50:
        return KEY_DOWN;
    case 0x51:
        return KEY_PAGEDOWN;
    case 0x52:
        return KEY_INSERT;
    case 0x53:
        return KEY_DELETE;
    case 0x3B:
        return KEY_F1;
    case 0x3C:
        return KEY_F2;
    case 0x3D:
        return KEY_F3;
    case 0x3E:
        return KEY_F4;
    case 0x3F:
        return KEY_F5;
    case 0x40:
        return KEY_F6;
    case 0x41:
        return KEY_F7;
    case 0x42:
        return KEY_F8;
    case 0x43:
        return KEY_F9;
    case 0x44:
        return KEY_F10;
    case 0x57:
        return KEY_F11;
    case 0x58:
        return KEY_F12;
    }

    // Попытка получить ASCII-символ
    if (scancode < sizeof(scancode_ascii))
    {
        char ch = shift ? scancode_shifted[scancode] : scancode_ascii[scancode];
        // Применяем CapsLock только к буквам
        if (capslock && (ch >= 'a' && ch <= 'z'))
            ch = ch - 'a' + 'A';
        else if (capslock && (ch >= 'A' && ch <= 'Z'))
            ch = ch - 'A' + 'a';
        if (ch != 0)
            return (uint8_t)ch;
    }

    return 0;
}

// ------------------------------------------------------------
// Callback'и для прерываний
// ------------------------------------------------------------

void keyboard_top_callback(isr_data_t data)
{
    uint8_t status = inb(KEYBOARD_STATUS_PORT);
    if (status & 0x01)
    {
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);
        if (scancode != 0)
        {
            scancode_buffer_push(scancode);
        }
    }
}

void keyboard_bottom_callback(isr_data_t data)
{
    uint8_t scancode;
    bool any_event = false;
    while (scancode_buffer_pop(&scancode))
    {

        keyboard_event_t ev;
        ev.pressed = !(scancode & 0x80);
        ev.scancode = scancode;
        update_modifiers(scancode, ev.pressed);
        ev.modifiers = current_modifiers;
        ev.keycode = scancode_to_keycode(scancode, current_modifiers);

        if (ev.keycode != 0)
        {
            if (event_queue_push(&ev))
            {
                any_event = true;
            }
            else
            {
                PANIC("KEYBOARD EVENT QUEUE OVERFLOW");
            }
        }
    }
    if (any_event)
    {
        task_event_flush(keyboard_event);
    }
}

// ------------------------------------------------------------
// Инициализация
// ------------------------------------------------------------

void keyboard_init()
{
    keyboard_event = malloc(sizeof(task_event_t));
    if (keyboard_event)
    {
        task_event_init(keyboard_event);
    }
    outb(KEYBOARD_STATUS_PORT, 0xAE); // включить клавиатуру
    interrupt_register(IRQ1, keyboard_top_callback, keyboard_bottom_callback);
}