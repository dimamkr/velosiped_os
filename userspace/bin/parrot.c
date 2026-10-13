#include "syscall.h"
#include "input.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("parrot: ready (Esc to quit)\n");

    keyboard_event_t ev;
    while (1)
    {
        int n = sys_read(0, &ev, sizeof(ev));
        if (n != (int)sizeof(ev))
        {
            printf("parrot: read failed\n");
            goto end;
        }

        if (!ev.pressed)
            continue;

        switch (ev.keycode)
        {
        case KEY_ESC:
            printf("\nparrot: bye\n");
            goto end;
        case KEY_ENTER:
            printf("\n");
            break;
        case KEY_BACKSPACE:
            printf("\b \b");
            break;
        case KEY_TAB:
            printf("\t");
            break;
        case KEY_SPACE:
            printf(" ");
            break;
        default:
            if (ev.keycode >= 0x20 && ev.keycode < 0x80)
                printf("%c", (char)ev.keycode);
            break;
        }
    }

end:
    return 0;
}
