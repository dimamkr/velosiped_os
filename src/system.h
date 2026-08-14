#ifndef SYSTEM
#define SYSTEM

#include "types.h"

#define BARRIER asm volatile("" ::: "memory")

void outb(uint16_t port, byte_t value);
byte_t inb(uint16_t port);
void uint32_to_string(uint32_t number, char *result);
bool_t strcmp(const char *a, const char *b);
void halt();

void memcpy(void *dst, const void *src, uint32_t size);

#define PANIC(msg) panic(msg, __FILE__, __LINE__);
#define ASSERT(b) ((b) ? (void)0 : panic_assert(#b, __FILE__, __LINE__))

void panic(char *msg, char *file, uint32_t line);
void panic_assert(char *msg, char *file, uint32_t line);

#endif