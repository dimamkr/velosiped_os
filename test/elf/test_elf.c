#include <stdint.h>

__attribute__((optimize("O0"))) void wait()
{
    for (uint32_t i = 0; i != (uint32_t)-1; ++i)
    {
    }
}

void page_fault_init(void)
{
    uint32_t *val = ((uint32_t *)(0xC0000000 + 0x10000));
    for (uint32_t i = 0; i < 1000; ++i)
    {
        *val = 3;
        ++val;
    }
}

__attribute__((optimize("O0"))) void _start(void)
{
    // char *msg = (char *)arg;
    char *msg = "HELLO I AM USER!!!\n";

    for (int i = 0; i < 3; ++i)
    {
        // вывод на экран
        asm volatile("int $0x80" : : "a"(1), "b"(1), "c"(msg), "d"(14));
        // wait();
    }

    // page_fault_init();

    // выход
    asm volatile("int $0x80" : : "a"(4), "b"(1), "c"(msg), "d"(14));
}
