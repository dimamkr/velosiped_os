void _start(void *arg)
{
    char *msg = (char *)arg;

    asm volatile("int $0x80" : : "a"(1), "b"(1), "c"(msg), "d"(14));
}
