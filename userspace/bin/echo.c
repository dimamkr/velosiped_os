#include <unistd.h>
#include <stdint.h>

uint32_t
strlen(const char *s)
{
    uint32_t len = 0;
    for (; s[len]; len++)
        ;
    return len;
}

void _start(int argc, char **argv)
{
    for (int i = 0; i < argc; ++i)
    {
        sys_write(1, argv[i], strlen(argv[i]) + 1);
    }

    sys_exit(0);
}