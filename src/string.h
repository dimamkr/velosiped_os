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
int strncmp(const char *a, const char *b, uint32_t n);
char *strdup(const char *src);
void strcat(char *a, const char *b);
void string_to_lower(char *s);
void string_to_upper(char *s);
char *strstr(const char *needle, const char *haystack);
void strcpy(char *dst, const char *src);
uint32_t strchr(const char *str, char chr);
uint32_t strchr_r(const char *str, char chr);
uint32_t wide_char_to_utf8(char *dst, const wchar_t *src, uint32_t dst_size);
uint32_t utf8_to_wide_char(wchar_t *dst, const char *src, uint32_t dst_size);
void uint32_to_string(uint32_t number, char *result, uint8_t base);
bool_t is_matching_pattern(const char *str, const char *pattern, bool_t ignore_case);
uint32_t strtoul(const char *str, const char **endsym, uint8_t base);

int toupper (int c); // для совместимости с std
int tolower (int c); // для совместимости с std
int isxdigit(int c); // для совместимости с std
int isprint(int c); // для совместимости с std

#endif