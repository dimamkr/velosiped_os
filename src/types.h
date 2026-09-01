#ifndef TYPES
#define TYPES

#include <stdbool.h>

// Типы данных
typedef unsigned char byte_t;
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short int uint16_t;
typedef signed short int int16_t;
typedef unsigned long int uint32_t;
typedef signed long int int32_t;
typedef unsigned long long uint64_t;
typedef signed long long int64_t;
typedef uint16_t wchar_t;

typedef bool bool_t;

#define NULL ((void *)0)
#define KB ((uint32_t)0x400)
#define MB ((uint32_t)0x100000)
#define GB ((uint32_t)0x40000000)

#define MAKEWORD(a, b) ((((uint16_t)(a)) << 8) | ((uint16_t)(b)))
#define MAKEDWORD(a, b) ((((uint32_t)(a)) << 16) | ((uint32_t)(b)))

#define PAIR(type1, type2) \
    struct                 \
    {                      \
        type1 first;       \
        type2 second;      \
    }

#define likely(x) __builtin_expect(!!(x), 1)   // Скорее всего ИСТИНА
#define unlikely(x) __builtin_expect(!!(x), 0) // Скорее всего ЛОЖЬ

#endif