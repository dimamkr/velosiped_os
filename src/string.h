#ifndef STRING
#define STRING

#include "types.h"
#include "system.h"
#include "dynamic_array.h"

#define LOWER(ch) ('A' <= (ch) && (ch) <= 'Z' ? ch + ('a' - 'A') : ch)
#define UPPER(ch) ('a' <= (ch) && (ch) <= 'z' ? ch - ('a' - 'A') : ch)
#define DIGIT_BY_INDEX(n) ((n) >= 10 ? 'a' + ((n) - 10) : '0' + (n))
#define INDEX_BY_DIGIT(c) ((c) >= 'a' ? (c) - 'a' + 10 : (c) - '0')

uint32_t strlen(const char *s);
int strcmp(const char *a, const char *b);
char *strdup(const char *src);
void strcat(char *a, const char *b);
void string_to_lower(char *s);
void string_to_upper(char *s);
uint32_t strstr(const char *haystack, const char *needle);
uint32_t wide_char_to_utf8(char *dst, const wchar_t *src, uint32_t dst_size);
void uint32_to_string(uint32_t number, char *result, uint8_t base);
bool_t is_matching_pattern(const char *str, const char *pattern);
uint32_t string_to_uint32(char *str, uint8_t base);

#endif