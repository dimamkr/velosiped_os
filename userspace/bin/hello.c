#include <unistd.h>

void _start(void)
{
    static const char msg[] = "Hello from userspace!\n";

    sys_write(1, msg, sizeof(msg));

    sys_exit(0);
}