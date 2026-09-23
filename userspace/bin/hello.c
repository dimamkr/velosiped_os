#include <unistd.h>

void _start(void)
{
    static const char msg[] = "Hello from userspace!\n";

    write(1, msg, sizeof(msg) - 1);
    exit(0);
}