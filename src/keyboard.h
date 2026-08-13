#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"
#include "isr.h"
#include "system.h"
#include "pic.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// Special keys
#define KEY_ENTER 0x1C
#define KEY_BACKSPACE 0x0E
#define KEY_ESC 0x01
#define KEY_LSHIFT 0x2A
#define KEY_RSHIFT 0x36
#define KEY_CAPSLOCK 0x3A
#define KEY_TAB 0x0F
#define KEY_CTRL 0x1D
#define KEY_ALT 0x38
#define KEY_SPACE 0x39

// Function keys
#define KEY_F1 0x3B
#define KEY_F2 0x3C
#define KEY_F3 0x3D
#define KEY_F4 0x3E
#define KEY_F5 0x3F
#define KEY_F6 0x40
#define KEY_F7 0x41
#define KEY_F8 0x42
#define KEY_F9 0x43
#define KEY_F10 0x44

// Arrow keys
#define KEY_UP 0x48
#define KEY_DOWN 0x50
#define KEY_LEFT 0x4B
#define KEY_RIGHT 0x4D

void keyboard_init(void);
bool_t keyboard_is_key_pressed(uint8_t scancode);
void keyboard_wait_for_key(uint8_t scancode);
void keyboard_enable_interrupts(void);
void keyboard_disable_interrupts(void);
bool_t keyboard_is_shift_pressed(void);
bool_t keyboard_is_capslock_on(void);
bool_t keyboard_is_ctrl_pressed(void);
bool_t keyboard_is_alt_pressed(void);

char keyboard_get_input(void);

#endif
