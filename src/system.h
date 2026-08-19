#ifndef SYSTEM
#define SYSTEM

#include "types.h"
#include "isr.h"

#define BARRIER asm volatile("" ::: "memory")

void uint32_to_string(uint32_t number, char *result);
bool_t strcmp(const char *a, const char *b);
uint32_t strlen(const char *s);

void memcpy(void *dst, const void *src, uint32_t size);
void memset(void *ptr, byte_t value, uint32_t size);
bool memcmp(void *ptr_a, void *ptr_b, uint32_t size);

#define PANIC(msg) panic(msg, __FILE__, __LINE__);
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

#endif