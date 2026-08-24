#include "keyboard.h"
#include "terminal.h"
#include "system.h"
#include "konsole.h"

// иначе компилятор может заменить call и ret на jump + leave, и это сломает стек, поскольку он управляется полностью вручную на ассемблере
#pragma GCC optimize("no-optimize-sibling-calls")

const char keyboard_scancodes_map[] =
    {
        '\0',
        0 /*escape*/,
        '1',
        '2',
        '3',
        '4',
        '5',
        '6',
        '7',
        '8',
        '9',
        '0',
        '-',
        '=',
        '\b' /*backspace*/,
        '\t' /*tab*/,
        'q',
        'w',
        'e',
        'r',
        't',
        'y',
        'u',
        'i',
        'o',
        'p',
        '[',
        ']',
        '\n' /*enter*/,
        0 /*left control*/,
        'a',
        's',
        'd',
        'f',
        'g',
        'h',
        'j',
        'k',
        'l',
        ';',
        '\'' /*single quote*/,
        '`' /*back tick*/,
        0 /*left shift*/,
        '\\',
        'z',
        'x',
        'c',
        'v',
        'b',
        'n',
        'm',
        ',',
        '.',
        '/',
        0 /*right shift*/,
        '*' /*(keypad)*/,
        0 /*left alt*/,
        ' ' /*space*/,
        0 /*CapsLock*/,
        0 /*F1*/,
        0 /*F2*/,
        0 /*F3*/,
        0 /*F4*/,
        0 /*F5*/,
        0 /*F6*/,
        0 /*F7*/,
        0 /*F8*/,
        0 /*F9*/,
        0 /*F10*/,
        0 /*NumberLock*/,
        0 /*ScrollLock*/,
        '7' /*(keypad)*/,
        '8' /*(keypad)*/,
        '9' /*(keypad)*/,
        '-' /*(keypad)*/,
        '4' /*(keypad)*/,
        '5' /*(keypad)*/,
        '6' /*(keypad)*/,
        '+' /*(keypad)*/,
        '1' /*(keypad)*/,
        '2' /*(keypad)*/,
        '3' /*(keypad)*/,
        '0' /*(keypad)*/,
        '.' /*(keypad)*/,
        0 /*F11*/,
        0 /*F12*/
};

// Максимально простой драйвер клавиатуры

// если забыть считать хоть при одном прерывании клавиатуры, они будут запрещены
static uint8_t keyboard_read_scancode(void)
{
    uint8_t scancode = 0;

    // Проверяем, есть ли данные в буфере
    uint8_t status = inb(KEYBOARD_STATUS_PORT);

    if (status & 0x01)
    { // Данные есть
        scancode = inb(KEYBOARD_DATA_PORT);
    }

    return scancode;
}

static char keyboard_scancode_to_ascii(uint8_t scancode)
{
    // Игнорируем отпускание клавиш (старший бит = 1)
    if (scancode & 0x80)
    {
        return 0;
    }

    if (scancode < sizeof(keyboard_scancodes_map))
    {
        return keyboard_scancodes_map[scancode];
    }

    return 0;
}

// 0 в случае отсутствия ввода
// также 0 при поднятии клавиши
char keyboard_get_input()
{
    uint8_t scancode = keyboard_read_scancode();
    if (scancode == 0)
    {
        return 0;
    }
    return keyboard_scancode_to_ascii(scancode);
}

static volatile void keyboard_callback(isr_data_t data)
{
    char symbol = keyboard_get_input();
    if (symbol != 0)
    {
        terminal_read_symbol(symbol);
    }
    // asm volatile("" ::: "memory");
}

void keyboard_init()
{
    outb(0x64, 0xAE);
    interrupt_register(IRQ1, keyboard_callback, NULL);
}
