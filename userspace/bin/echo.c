#include <unistd.h>
#include <stdint.h>

__attribute__((optimize("O3,unroll-loops")))
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
        write(1, argv[i], strlen(argv[i]) + 1);
    }

    exit(0);
}