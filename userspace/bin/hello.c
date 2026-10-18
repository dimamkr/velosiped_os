#include <unistd.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    static const char msg[] = "Hello from userspace!\n";

    printf("%s\n", msg);

    return 0;
}