#ifndef SYSTEM
#define SYSTEM

#include "types.h"
#include "isr.h"

#define BARRIER asm volatile("" ::: "memory")
#define max(a, b) (_Generic((a), uint8_t: _uint8_max, uint16_t: _uint16_max, uint32_t: _uint32_max))((a), (b))
#define min(a, b) (_Generic((a), uint8_t: _uint8_min, uint16_t: _uint16_min, uint32_t: _uint32_min))((a), (b))

void memcpy(void *dst, const void *src, uint32_t size);
void memset(void *ptr, byte_t value, uint32_t size);
bool_t memcmp(void *ptr_a, void *ptr_b, uint32_t size);
void memswap(void *buff_1, void *buff_2, uint32_t size);

#define PANIC(msg) panic(msg, __FILE__, __LINE__)
#define ASSERT(b) ((b) ? (void)0 : panic_assert(#b, __FILE__, __LINE__))

void panic(char *msg, char *file, uint32_t line);
void panic_assert(char *msg, char *file, uint32_t line);
void halt();
void outb(uint16_t port, byte_t value);
byte_t inb(uint16_t port);
void outw(uint16_t port, uint16_t value);
uint16_t inw(uint16_t port);
void outl(uint16_t port, uint32_t value);
uint32_t inl(uint16_t port);


__attribute__((always_inline, artificial))
inline uint8_t _uint8_max (uint8_t a, uint8_t b)
{
    return a > b ? a : b;
}

__attribute__((always_inline, artificial))
inline uint8_t _uint8_min (uint8_t a, uint8_t b)
{
    return a < b ? a : b;
}

__attribute__((always_inline, artificial))
inline uint16_t _uint16_max (uint16_t a, uint16_t b)
{
    return a > b ? a : b;
}

__attribute__((always_inline, artificial))
inline uint16_t _uint16_min (uint16_t a, uint16_t b)
{
    return a < b ? a : b;
}

__attribute__((always_inline, artificial))
inline uint32_t _uint32_max (uint32_t a, uint32_t b)
{
    return a > b ? a : b;
}

__attribute__((always_inline, artificial))
inline uint32_t _uint32_min (uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}

#endif