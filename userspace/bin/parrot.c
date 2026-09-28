#include "unistd.h"
#include "input.h"

static void putc_(char c) { sys_write(1, &c, 1); }
static void puts_(const char *s, unsigned n) { sys_write(1, s, n); }

void _start(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    puts_("parrot: ready (Esc to quit)\n", 28);

    keyboard_event_t ev;
    while (1)
    {
        int n = sys_read(0, &ev, sizeof(ev));
        if (n != (int)sizeof(ev))
        {
            puts_("parrot: read failed\n", 21);
            sys_exit(1);
        }

        if (!ev.pressed)
            continue;

        switch (ev.keycode)
        {
        case KEY_ESC:
            puts_("\nparrot: bye\n", 14);
            sys_exit(0);
            break;
        case KEY_ENTER:
            putc_('\n');
            break;
        case KEY_BACKSPACE:
            puts_("\b \b", 3);
            break;
        case KEY_TAB:
            putc_('\t');
            break;
        case KEY_SPACE:
            putc_(' ');
            break;
        default:
            if (ev.keycode >= 0x20 && ev.keycode < 0x80)
                putc_((char)ev.keycode);
            break;
        }
    }
    sys_exit(0);
}